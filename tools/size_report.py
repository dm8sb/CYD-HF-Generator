#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Firmware size report for CI (ARBEITSPLAN G7). Standard library only.

measure  reads the ELF and the partition table of each PlatformIO build and writes size.json.
         Used bytes are summed per section exactly like PlatformIO's size check for espressif32
         (SIZEPROGREGEXP / SIZEDATAREGEXP on `size -A`), so the numbers match the
         "RAM: ... used X bytes from Y bytes" / "Flash: ..." lines of `pio run`. The flash total is
         the first factory/ota_0 app partition (as PlatformIO picks it), the RAM total is the
         board's upload.maximum_ram_size. With --log-dir the values are cross-checked against
         the lines PlatformIO printed in <log-dir>/<env>.log (mismatch: warning; with --strict
         also exit code 3).
report   prints a Markdown table of size.json, with the difference to a baseline size.json.

Examples:
  python tools/size_report.py measure -e esp32-2432S028R -o size.json
  python tools/size_report.py report size.json --baseline base/size.json --baseline-label "main @ 1a2b3c4"
  python tools/size_report.py --self-test
"""

import argparse
import json
import os
import re
import struct
import sys

SCHEMA = 1

# PlatformIO espressif32 (6.x and 8.x builder/main.py): sections counted for Flash and RAM.
FLASH_SECTIONS = (".iram0.text", ".iram0.vectors", ".dram0.data", ".flash.text", ".flash.rodata")
RAM_SECTIONS = (".dram0.data", ".dram0.bss", ".noinit")
# boards/esp32dev.json upload.maximum_ram_size (both platforms).
DEFAULT_RAM_TOTAL = 327680

PIO_LINE = re.compile(r"^(RAM|Flash):\s.*\(used (\d+) bytes from (\d+) bytes\)", re.M)


class SizeError(Exception):
    pass


# ---------------------------------------------------------------- ELF / partition table

def elf_section_sizes(data):
    """Return [(name, size), ...] of all sections of an ELF32/ELF64 image (bytes)."""
    if len(data) < 52 or data[:4] != b"\x7fELF":
        raise SizeError("not an ELF file")
    elf_class, endian = data[4], data[5]
    if elf_class not in (1, 2) or endian not in (1, 2):
        raise SizeError("unsupported ELF class or byte order")
    e = "<" if endian == 1 else ">"
    try:
        if elf_class == 1:
            shoff, = struct.unpack_from(e + "I", data, 0x20)
            shentsize, shnum, shstrndx = struct.unpack_from(e + "HHH", data, 0x2E)
            shfmt, name_at, size_fmt, size_at, off_fmt, off_at = e + "I", 0, e + "I", 20, e + "I", 16
        else:
            shoff, = struct.unpack_from(e + "Q", data, 0x28)
            shentsize, shnum, shstrndx = struct.unpack_from(e + "HHH", data, 0x3A)
            shfmt, name_at, size_fmt, size_at, off_fmt, off_at = e + "I", 0, e + "Q", 32, e + "Q", 24

        def field(index, fmt, at):
            return struct.unpack_from(fmt, data, shoff + index * shentsize + at)[0]

        if shnum == 0 or shstrndx >= shnum:
            raise SizeError("ELF has no section name table")
        str_off = field(shstrndx, off_fmt, off_at)
        str_size = field(shstrndx, size_fmt, size_at)
        names = data[str_off:str_off + str_size]
        result = []
        for i in range(shnum):
            name_off = field(i, shfmt, name_at)
            end = names.find(b"\0", name_off)
            name = names[name_off:end if end >= 0 else len(names)].decode("ascii", "replace")
            result.append((name, field(i, size_fmt, size_at)))
        return result
    except struct.error:
        raise SizeError("truncated ELF file")


def used_bytes(sections):
    flash = sum(size for name, size in sections if name in FLASH_SECTIONS)
    ram = sum(size for name, size in sections if name in RAM_SECTIONS)
    return flash, ram


def app_partition_size(data):
    """Size of the first factory/ota_0 app partition in a binary partition table."""
    for off in range(0, len(data) - 31, 32):
        magic, ptype, subtype, _offset, size = struct.unpack_from("<HBBII", data, off)
        if magic != 0x50AA:  # 0xEBEB = MD5 entry, 0xFFFF = end
            break
        if ptype == 0 and subtype in (0x00, 0x10):  # app: factory, ota_0
            return size
    raise SizeError("no factory/ota_0 app partition in the partition table")


def parse_pio_log(text):
    """{'ram': (used, total), 'flash': (used, total)} from the last size lines PlatformIO printed."""
    found = {}
    for kind, used, total in PIO_LINE.findall(text):
        found[kind.lower()] = (int(used), int(total))
    return found


# ---------------------------------------------------------------- measure

def measure_env(env, build_dir, ram_total):
    folder = os.path.join(build_dir, env)
    try:
        with open(os.path.join(folder, "firmware.elf"), "rb") as f:
            flash_used, ram_used = used_bytes(elf_section_sizes(f.read()))
        with open(os.path.join(folder, "partitions.bin"), "rb") as f:
            flash_total = app_partition_size(f.read())
    except OSError as exc:
        raise SizeError("%s: %s" % (env, exc))
    return {"env": env, "flash_used": flash_used, "flash_total": flash_total,
            "ram_used": ram_used, "ram_total": ram_total}


def cross_check(entry, log_text):
    """List of mismatches between the measured entry and PlatformIO's printed lines."""
    lines = parse_pio_log(log_text)
    problems = []
    for kind in ("flash", "ram"):
        if kind not in lines:
            problems.append("%s: no '%s:' size line in the build log" % (entry["env"], kind.capitalize()))
            continue
        mine = (entry[kind + "_used"], entry[kind + "_total"])
        if mine != lines[kind]:
            problems.append("%s: %s measured %d/%d, PlatformIO printed %d/%d"
                            % ((entry["env"], kind) + mine + lines[kind]))
    return problems


def cmd_measure(args):
    report = {"schema": SCHEMA, "commit": args.commit or "", "ref": args.ref or "", "envs": []}
    problems = []
    for env in args.env:
        entry = measure_env(env, args.build_dir, args.ram_total)
        report["envs"].append(entry)
        if args.log_dir:
            try:
                with open(os.path.join(args.log_dir, env + ".log"), encoding="utf-8", errors="replace") as f:
                    problems += cross_check(entry, f.read())
            except OSError as exc:
                problems.append("%s: %s" % (env, exc))
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    for p in problems:
        print("::warning title=size report::%s" % p if args.github else "warning: " + p, file=sys.stderr)
    return 3 if problems and args.strict else 0


# ---------------------------------------------------------------- report

def load_report(path):
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict) or data.get("schema") != SCHEMA or not isinstance(data.get("envs"), list):
        raise SizeError("%s: not a size report (schema %s)" % (path, SCHEMA))
    for e in data["envs"]:
        for key in ("flash_used", "flash_total", "ram_used", "ram_total"):
            if not isinstance(e.get(key), int) or isinstance(e.get(key), bool):
                raise SizeError("%s: %s has no integer %s" % (path, e.get("env"), key))
    return data


def fmt_bytes(n):
    return "{:,}".format(n)


def fmt_used(used, total):
    return "%s / %s B (%.1f%%)" % (fmt_bytes(used), fmt_bytes(total), 100.0 * used / total if total else 0.0)


def fmt_delta(now, before):
    if before is None:
        return "new"
    d = now - before
    if d == 0:
        return "±0 B"
    pct = " (%+.2f%%)" % (100.0 * d / before) if before else ""
    return "%+d B%s" % (d, pct)


def render(current, baseline=None, label="", no_baseline_reason=""):
    lines = ["### Firmware size", ""]
    if current.get("commit"):
        lines.append("Commit `%s`%s." % (current["commit"][:12],
                                          " on `%s`" % current["ref"] if current.get("ref") else ""))
    if baseline is not None:
        lines.append("Baseline: %s." % (label or "`%s`" % baseline.get("commit", "?")[:12]))
    else:
        lines.append("No baseline%s." % (": " + no_baseline_reason if no_baseline_reason else ""))
    lines.append("")
    base = {e.get("env"): e for e in (baseline or {}).get("envs", [])}
    if baseline is not None:
        lines += ["| Env | Flash | Δ Flash | RAM | Δ RAM |", "|---|---:|---:|---:|---:|"]
    else:
        lines += ["| Env | Flash | RAM |", "|---|---:|---:|"]
    for e in current["envs"]:
        row = ["`%s`" % e["env"], fmt_used(e["flash_used"], e["flash_total"])]
        if baseline is not None:
            b = base.get(e["env"])
            row.append(fmt_delta(e["flash_used"], b["flash_used"] if b else None))
        row.append(fmt_used(e["ram_used"], e["ram_total"]))
        if baseline is not None:
            row.append(fmt_delta(e["ram_used"], b["ram_used"] if b else None))
        lines.append("| " + " | ".join(row) + " |")
    lines += ["", "Flash and RAM as PlatformIO counts them (static RAM: `.dram0.data` + `.dram0.bss` + `.noinit`)."]
    return "\n".join(lines) + "\n"


def cmd_report(args):
    current = load_report(args.current)
    baseline, reason = None, args.no_baseline_reason or ""
    if args.baseline:
        if os.path.exists(args.baseline):
            try:
                baseline = load_report(args.baseline)
            except (SizeError, ValueError) as exc:
                reason = "unreadable baseline (%s)" % exc
        elif not reason:
            reason = "no size.json from the base"
    text = render(current, baseline, args.baseline_label or "", reason)
    if args.output:
        with open(args.output, "a", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


# ---------------------------------------------------------------- self-test

def _fake_elf(sections, endian="<"):
    """Minimal ELF32 with the given (name, size) sections (no contents)."""
    names = b"\0" + b"".join(n.encode() + b"\0" for n, _ in sections) + b".shstrtab\0"
    shoff = 52 + len(names)
    head = bytearray(52)
    head[0:6] = b"\x7fELF\x01" + (b"\x01" if endian == "<" else b"\x02")
    struct.pack_into(endian + "I", head, 0x20, shoff)
    struct.pack_into(endian + "HHH", head, 0x2E, 40, len(sections) + 2, len(sections) + 1)
    table = bytearray(40)  # null section
    pos = 1
    for name, size in sections:
        sh = bytearray(40)
        struct.pack_into(endian + "I", sh, 0, pos)
        struct.pack_into(endian + "I", sh, 20, size)
        table += sh
        pos += len(name) + 1
    sh = bytearray(40)
    struct.pack_into(endian + "I", sh, 0, pos)
    struct.pack_into(endian + "I", sh, 16, 52)
    struct.pack_into(endian + "I", sh, 20, len(names))
    table += sh
    return bytes(head) + names + bytes(table)


def self_test():
    import tempfile
    import unittest

    secs = [(".iram0.vectors", 1024), (".iram0.text", 60000), (".dram0.data", 20000), (".noinit", 16),
            (".dram0.bss", 50000), (".flash.text", 900000), (".flash.rodata", 300000),
            (".flash.rodata_noload", 7), (".flash.appdesc", 256), (".debug_info", 999999)]
    flash_expected = 1024 + 60000 + 20000 + 900000 + 300000
    ram_expected = 20000 + 16 + 50000

    def ptable(entries):
        out = b""
        for ptype, sub, off, size in entries:
            out += struct.pack("<HBBII16sI", 0x50AA, ptype, sub, off, size, b"x", 0)
        return out + struct.pack("<H", 0xEBEB) + b"\0" * 30 + b"\xff" * 32

    class T(unittest.TestCase):
        def test_elf_sections_little_and_big_endian(self):
            for endian in "<>":
                got = elf_section_sizes(_fake_elf(secs, endian))
                self.assertEqual(got[1:-1], secs)
                self.assertEqual(used_bytes(got), (flash_expected, ram_expected))

        def test_not_elf(self):
            self.assertRaises(SizeError, elf_section_sizes, b"MZ" + b"\0" * 60)
            self.assertRaises(SizeError, elf_section_sizes, _fake_elf(secs)[:60])

        def test_partition_huge_app(self):
            # huge_app.csv: nvs, otadata, app0 (ota_0? no: factory) 3M, spiffs
            data = ptable([(1, 2, 0x9000, 0x5000), (1, 0, 0xE000, 0x2000),
                           (0, 0x00, 0x10000, 0x300000), (1, 0x82, 0x310000, 0xE0000)])
            self.assertEqual(app_partition_size(data), 3145728)

        def test_partition_first_ota0(self):
            data = ptable([(0, 0x10, 0x10000, 0x140000), (0, 0x11, 0x150000, 0x140000)])
            self.assertEqual(app_partition_size(data), 0x140000)
            self.assertRaises(SizeError, app_partition_size, ptable([(1, 2, 0x9000, 0x5000)]))

        def test_pio_log_and_cross_check(self):
            log = ("Checking size .pio/build/x/firmware.elf\n"
                   "RAM:   [==        ]  23.3% (used 76476 bytes from 327680 bytes)\n"
                   "Flash: [====      ]  43.9% (used 1381971 bytes from 3145728 bytes)\n")
            self.assertEqual(parse_pio_log(log), {"ram": (76476, 327680), "flash": (1381971, 3145728)})
            entry = {"env": "x", "flash_used": 1381971, "flash_total": 3145728,
                     "ram_used": 76476, "ram_total": 327680}
            self.assertEqual(cross_check(entry, log), [])
            entry["ram_used"] += 1
            self.assertEqual(len(cross_check(entry, log)), 1)
            self.assertEqual(len(cross_check(entry, "no lines")), 2)

        def test_measure_and_report_round_trip(self):
            with tempfile.TemporaryDirectory() as tmp:
                os.makedirs(os.path.join(tmp, "e1"))
                with open(os.path.join(tmp, "e1", "firmware.elf"), "wb") as f:
                    f.write(_fake_elf(secs))
                with open(os.path.join(tmp, "e1", "partitions.bin"), "wb") as f:
                    f.write(ptable([(0, 0, 0x10000, 0x300000)]))
                entry = measure_env("e1", tmp, DEFAULT_RAM_TOTAL)
                self.assertEqual(entry, {"env": "e1", "flash_used": flash_expected, "flash_total": 0x300000,
                                         "ram_used": ram_expected, "ram_total": DEFAULT_RAM_TOTAL})
                self.assertRaises(SizeError, measure_env, "missing", tmp, DEFAULT_RAM_TOTAL)
                path = os.path.join(tmp, "size.json")
                with open(path, "w") as f:
                    json.dump({"schema": SCHEMA, "commit": "abc", "ref": "b", "envs": [entry]}, f)
                self.assertEqual(load_report(path)["envs"][0], entry)
                with open(path, "w") as f:
                    json.dump({"schema": SCHEMA, "envs": [{"env": "e1", "flash_used": "1"}]}, f)
                self.assertRaises(SizeError, load_report, path)

        def test_render_with_and_without_baseline(self):
            cur = {"schema": 1, "commit": "0123456789abcdef", "ref": "feature/x", "envs": [
                {"env": "a", "flash_used": 1001000, "flash_total": 3145728, "ram_used": 76476, "ram_total": 327680},
                {"env": "b", "flash_used": 10, "flash_total": 100, "ram_used": 5, "ram_total": 10}]}
            base = {"schema": 1, "commit": "fedcba", "envs": [
                {"env": "a", "flash_used": 1000000, "flash_total": 3145728, "ram_used": 76476, "ram_total": 327680}]}
            text = render(cur, base, "`main` @ fedcba")
            self.assertIn("Baseline: `main` @ fedcba.", text)
            self.assertIn("| `a` | 1,001,000 / 3,145,728 B (31.8%) | +1000 B (+0.10%) | "
                          "76,476 / 327,680 B (23.3%) | ±0 B |", text)
            self.assertIn("| `b` | 10 / 100 B (10.0%) | new | 5 / 10 B (50.0%) | new |", text)
            text = render(cur, None, "", "no successful run on `main`")
            self.assertIn("No baseline: no successful run on `main`.", text)
            self.assertIn("| `a` | 1,001,000 / 3,145,728 B (31.8%) | 76,476 / 327,680 B (23.3%) |", text)
            self.assertEqual(fmt_delta(90, 100), "-10 B (-10.00%)")

    result = unittest.TextTestRunner(verbosity=1).run(unittest.defaultTestLoader.loadTestsFromTestCase(T))
    return 0 if result.wasSuccessful() else 1


# ---------------------------------------------------------------- main

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--self-test", action="store_true", help="run the built-in tests and exit")
    sub = parser.add_subparsers(dest="cmd")

    m = sub.add_parser("measure", help="measure PlatformIO builds and write size.json")
    m.add_argument("-e", "--env", action="append", required=True, help="PlatformIO env (repeatable)")
    m.add_argument("--build-dir", default=os.path.join(".pio", "build"), help="default: .pio/build")
    m.add_argument("--ram-total", type=int, default=DEFAULT_RAM_TOTAL,
                   help="board upload.maximum_ram_size (default %d, esp32dev)" % DEFAULT_RAM_TOTAL)
    m.add_argument("--log-dir", help="cross-check against PlatformIO's lines in <dir>/<env>.log")
    m.add_argument("--commit", help="commit to record")
    m.add_argument("--ref", help="branch or ref to record")
    m.add_argument("--strict", action="store_true", help="exit code 3 when the cross-check fails")
    m.add_argument("--github", action="store_true", help="print warnings as GitHub annotations")
    m.add_argument("-o", "--output", help="size.json path (default: stdout)")

    r = sub.add_parser("report", help="print size.json as a Markdown table, with the difference to a baseline")
    r.add_argument("current", help="size.json of this build")
    r.add_argument("--baseline", help="size.json of the base (missing file = no baseline)")
    r.add_argument("--baseline-label", help="Markdown text naming the baseline")
    r.add_argument("--no-baseline-reason", help="text shown when there is no baseline")
    r.add_argument("-o", "--output", help="append to this file (e.g. $GITHUB_STEP_SUMMARY)")

    args = parser.parse_args(argv)
    try:  # the table has non-ASCII signs (Δ, ±); a Windows console may default to cp1252
        sys.stdout.reconfigure(encoding="utf-8")
    except (AttributeError, ValueError):
        pass
    if args.self_test:
        return self_test()
    try:
        if args.cmd == "measure":
            return cmd_measure(args)
        if args.cmd == "report":
            return cmd_report(args)
    except SizeError as exc:
        print("error: %s" % exc, file=sys.stderr)
        return 2
    parser.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
