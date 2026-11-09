#!/usr/bin/env python3
"""Web flasher site (ARBEITSPLAN G1/G2): collect the flash parts, assemble and check the site.

Python 3 standard library only. Used by .github/workflows/release.yml and locally:

  pio project metadata -e esp32-2432S028R-core3 --json-output > meta.json
  python tools/flasher_site.py collect --metadata meta.json --env esp32-2432S028R-core3 --out parts
  python tools/flasher_site.py assemble --parts parts --version 1.4.0 --commit abc1234 --out site
  python tools/flasher_site.py check site [--verify-cdn]
  python tools/flasher_site.py lock 10.4.0      # rewrite flasher/esp-web-tools.json (version bump)

collect: the offsets come from PlatformIO itself (FLASH_EXTRA_IMAGES of the framework build
script and the application offset of the board), not from this script. The parts stay separate:
a merged image from 0 would pad the gaps with 0xFF and erase NVS on every update.

check: manifest and files, every part an ESP32 image or the partition table where expected,
offsets sector-aligned, ascending and non-overlapping (erase granularity 4 KiB), every part inside
the bootloader area, the partition table sector or an app/otadata partition of the flashed
partition table (so an update never erases NVS), checksums, page template fully filled, the
CSP hash of the import map, and with --verify-cdn the pinned ESP Web Tools modules against the CDN.
"""
import argparse
import base64
import hashlib
import html
import json
import os
import re
import shutil
import struct
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE_DIR = os.path.join(ROOT, "flasher")
LOCK_FILE = os.path.join(TEMPLATE_DIR, "esp-web-tools.json")
SECTOR = 0x1000
PT_OFFSET = 0x8000
PT_MAX = 0xC00                    # gen_esp32part.py: table plus MD5, rest of the sector stays free
BOOTLOADER_OFFSET = 0x1000        # ESP32 (not S3/C3: those start at 0)
FLASH_SIZES = {0: 1 << 20, 1: 2 << 20, 2: 4 << 20, 3: 8 << 20, 4: 16 << 20}
VERSION_RE = re.compile(r"^[0-9A-Za-z][0-9A-Za-z.+-]{0,63}$")
JSDELIVR = "https://cdn.jsdelivr.net/npm/esp-web-tools@{v}/dist/web/"
JSDELIVR_LIST = "https://data.jsdelivr.com/v1/packages/npm/esp-web-tools@{v}?structure=flat"


class CheckError(Exception):
    pass


def fail(msg):
    raise CheckError(msg)


def sri(data):
    return "sha384-" + base64.b64encode(hashlib.sha384(data).digest()).decode()


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 16), b""):
            h.update(block)
    return h.hexdigest()


def read_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def write_text(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "cyd-flasher-site-check"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


# ---------------------------------------------------------------------------------------------
# collect


def cmd_collect(args):
    meta = read_json(args.metadata)
    if args.env not in meta:
        fail("env %s not in %s (has: %s)" % (args.env, args.metadata, ", ".join(meta)))
    env = meta[args.env]
    extra = env.get("extra") or {}
    images = [(int(i["offset"], 0), i["path"]) for i in extra.get("flash_images", [])]
    if "application_offset" not in extra:
        fail("metadata has no application_offset")
    firmware = os.path.join(os.path.dirname(env["prog_path"]), "firmware.bin")
    images.append((int(extra["application_offset"], 0), firmware))
    images.sort()
    os.makedirs(args.out, exist_ok=True)
    parts = []
    for offset, path in images:
        name = os.path.basename(path)
        if any(p["file"] == name for p in parts):
            fail("two parts named %s" % name)
        shutil.copyfile(path, os.path.join(args.out, name))
        parts.append({"file": name, "offset": offset, "size": os.path.getsize(path)})
        print("0x%06X  %-16s %8d B  <- %s" % (offset, name, parts[-1]["size"], path))
    write_text(os.path.join(args.out, "parts.json"),
               json.dumps({"env": args.env, "parts": parts}, indent=2) + "\n")


# ---------------------------------------------------------------------------------------------
# assemble


def import_map(lock):
    base = lock["base"]
    integrity = {base + name: h for name, h in sorted(lock["integrity"].items()) if name != lock["entry"]}
    return json.dumps({"imports": {}, "integrity": integrity}, indent=1)


def cmd_assemble(args):
    if not VERSION_RE.match(args.version):
        fail("version %r: only letters, digits and . + - allowed" % args.version)
    lock = read_json(LOCK_FILE)
    info = read_json(os.path.join(args.parts, "parts.json"))
    os.makedirs(args.out, exist_ok=True)
    fw_rel = "firmware/%s" % args.version   # versioned: a cached manifest never mixes releases
    fw_dir = os.path.join(args.out, *fw_rel.split("/"))
    os.makedirs(fw_dir, exist_ok=True)

    manifest = read_json(os.path.join(TEMPLATE_DIR, "manifest.template.json"))
    manifest["version"] = args.version
    rows, sums = [], []
    for p in info["parts"]:
        shutil.copyfile(os.path.join(args.parts, p["file"]), os.path.join(fw_dir, p["file"]))
        manifest["builds"][0]["parts"].append({"path": "%s/%s" % (fw_rel, p["file"]), "offset": p["offset"]})
        rows.append("<tr><td>0x%05X</td><td>%s</td><td>%d</td></tr>" % (p["offset"], html.escape(p["file"]), p["size"]))
        sums.append("%s  %s\n" % (sha256_file(os.path.join(fw_dir, p["file"])), p["file"]))
    write_text(os.path.join(fw_dir, "SHA256SUMS"), "".join(sums))
    write_text(os.path.join(args.out, "manifest.json"), json.dumps(manifest, indent=2) + "\n")

    imap = import_map(lock)
    with open(os.path.join(TEMPLATE_DIR, "index.html"), "r", encoding="utf-8") as f:
        page = f.read()
    fields = {
        "EWT_BASE": lock["base"],
        "EWT_ENTRY": lock["entry"],
        "EWT_ENTRY_INTEGRITY": lock["integrity"][lock["entry"]],
        "EWT_VERSION": html.escape(lock["version"]),
        "IMPORTMAP": imap,
        "IMPORTMAP_HASH": sri(imap.encode("utf-8")),
        "VERSION": html.escape(args.version),
        "COMMIT": html.escape(args.commit),
        "PARTS_ROWS": "\n".join(rows),
        "SUMS_PATH": fw_rel + "/SHA256SUMS",
        "REPO_URL": html.escape("https://github.com/" + args.repo),
        "REPO_NAME": html.escape(args.repo),
    }
    page = re.sub(r"\{\{([A-Z_]+)\}\}", lambda m: fields[m.group(1)], page)
    write_text(os.path.join(args.out, "index.html"), page)
    shutil.copyfile(os.path.join(TEMPLATE_DIR, "style.css"), os.path.join(args.out, "style.css"))
    print("site assembled in %s (version %s, %d parts)" % (args.out, args.version, len(rows)))


# ---------------------------------------------------------------------------------------------
# check


def parse_partition_table(data):
    parts = []
    for i in range(0, len(data) - 31, 32):
        entry = data[i:i + 32]
        magic = entry[:2]
        if magic == b"\xff\xff":
            return parts
        if magic == b"\xeb\xeb":          # MD5 of the table
            if hashlib.md5(data[:i]).digest() != entry[16:32]:
                fail("partition table: MD5 mismatch")
            continue
        if magic != b"\xaa\x50":
            fail("partition table: bad entry magic at %d" % i)
        ptype, subtype, offset, size = struct.unpack_from("<BBII", entry, 2)
        label = entry[12:28].split(b"\0", 1)[0].decode("ascii", "replace")
        parts.append({"label": label, "type": ptype, "subtype": subtype, "offset": offset, "size": size})
    fail("partition table: no end marker")


def esp_image_header(data, what):
    if len(data) < 24 or data[0] != 0xE9:
        fail("%s: not an ESP image (magic 0x%02X)" % (what, data[0] if data else 0))
    chip_id = struct.unpack_from("<H", data, 12)[0]
    if chip_id != 0:
        fail("%s: chip id %d, expected 0 (ESP32)" % (what, chip_id))
    return FLASH_SIZES.get(data[3] >> 4)


def check_site(site, verify_cdn=False, partitions_csv=None):
    def path_in_site(rel):
        if not rel or rel.startswith(("/", "\\")) or ".." in rel.split("/") or "\\" in rel or ":" in rel:
            fail("path %r must be relative inside the site" % rel)
        p = os.path.join(site, *rel.split("/"))
        if not os.path.isfile(p):
            fail("missing file %s" % rel)
        return p

    manifest = read_json(path_in_site("manifest.json"))
    for key in ("name", "version", "builds"):
        if key not in manifest:
            fail("manifest: no %s" % key)
    if manifest.get("new_install_prompt_erase") is not True:
        fail("manifest: new_install_prompt_erase must be true")
    if not VERSION_RE.match(str(manifest["version"])):
        fail("manifest: bad version %r" % manifest["version"])
    if len(manifest["builds"]) != 1 or manifest["builds"][0].get("chipFamily") != "ESP32":
        fail("manifest: expected exactly one build with chipFamily ESP32")
    parts = manifest["builds"][0].get("parts") or []
    if len(parts) < 2:
        fail("manifest: too few parts")

    loaded = []
    for p in parts:
        off = p.get("offset")
        if not isinstance(off, int) or isinstance(off, bool) or off < 0:
            fail("manifest: offset of %s must be a non-negative integer" % p.get("path"))
        with open(path_in_site(p.get("path", "")), "rb") as f:
            data = f.read()
        if not data:
            fail("%s is empty" % p["path"])
        loaded.append((off, p["path"], data))
    offsets = [o for o, _, _ in loaded]
    if offsets != sorted(offsets):
        fail("manifest: offsets not ascending")

    # Partition table: the part at 0x8000.
    pt = [d for o, _, d in loaded if o == PT_OFFSET]
    if not pt:
        fail("no partition table at 0x8000")
    if len(pt[0]) > PT_MAX:
        fail("partition table larger than 0x%X" % PT_MAX)
    table = parse_partition_table(pt[0])
    if not table:
        fail("partition table is empty")
    flash_end = max(e["offset"] + e["size"] for e in table)

    def describe(e):
        return "%s (0x%X..0x%X)" % (e["label"], e["offset"], e["offset"] + e["size"])

    prev_end = 0
    bootloader_flash = None
    app_seen = False
    for off, rel, data in loaded:
        end = off + len(data)
        erase_end = (end + SECTOR - 1) // SECTOR * SECTOR
        if off % SECTOR:
            fail("%s: offset 0x%X not sector-aligned" % (rel, off))
        if off < prev_end:
            fail("%s at 0x%X overlaps the previous part (erased up to 0x%X)" % (rel, off, prev_end))
        prev_end = erase_end
        if off == BOOTLOADER_OFFSET:
            bootloader_flash = esp_image_header(data, rel)
            if erase_end > PT_OFFSET:
                fail("%s: bootloader reaches 0x%X, beyond 0x8000" % (rel, end))
            region = "bootloader area 0x1000..0x8000"
        elif off == PT_OFFSET:
            first = min(e["offset"] for e in table)
            if erase_end > first:
                fail("%s: erase up to 0x%X reaches the first partition at 0x%X" % (rel, erase_end, first))
            region = "partition table sector"
        else:
            hosts = [e for e in table if e["offset"] <= off < e["offset"] + e["size"]]
            if not hosts:
                fail("%s at 0x%X lies in no partition" % (rel, off))
            e = hosts[0]
            is_app = e["type"] == 0x00
            is_otadata = e["type"] == 0x01 and e["subtype"] == 0x00
            if not (is_app or is_otadata):
                fail("%s at 0x%X would overwrite data partition %s" % (rel, off, describe(e)))
            if off != e["offset"]:
                fail("%s does not start at partition %s" % (rel, describe(e)))
            if erase_end > e["offset"] + e["size"]:
                fail("%s (%d B) does not fit partition %s" % (rel, len(data), describe(e)))
            if is_app:
                esp_image_header(data, rel)
                app_seen = True
            region = "partition " + describe(e)
        print("  0x%06X  %-34s %8d B  erase to 0x%06X  in %s" % (off, rel, len(data), erase_end, region))
    if bootloader_flash is None:
        fail("no bootloader at 0x%X" % BOOTLOADER_OFFSET)
    if not app_seen:
        fail("no application image")
    if flash_end > bootloader_flash:
        fail("partition table ends at 0x%X, beyond the flash size 0x%X of the bootloader header"
             % (flash_end, bootloader_flash))
    nvs = [describe(e) for e in table if e["type"] == 0x01 and e["subtype"] == 0x02]
    print("  NVS untouched: %s" % (", ".join(nvs) or "(no NVS partition)"))

    if partitions_csv:
        rows = []
        with open(partitions_csv, "r", encoding="utf-8") as f:
            for line in f:
                line = line.split("#", 1)[0].strip()
                if line:
                    c = [x.strip() for x in line.split(",")]
                    rows.append((c[0], int(c[3], 0), int(c[4], 0)))
        got = [(e["label"], e["offset"], e["size"]) for e in table]
        if rows != got:
            fail("partition table differs from %s: %s vs %s" % (partitions_csv, got, rows))
        print("  partition table matches %s" % os.path.basename(partitions_csv))

    # Checksums next to the parts.
    dirs = sorted({os.path.dirname(rel) for _, rel, _ in loaded})
    for d in dirs:
        sums_rel = (d + "/" if d else "") + "SHA256SUMS"
        listed = {}
        with open(path_in_site(sums_rel), "r", encoding="utf-8") as f:
            for line in f:
                h, name = line.rstrip("\n").split("  ", 1)
                listed[name] = h
        for _, rel, data in loaded:
            if os.path.dirname(rel) == d:
                name = os.path.basename(rel)
                if listed.get(name) != hashlib.sha256(data).hexdigest():
                    fail("%s: SHA256SUMS entry missing or wrong" % rel)

    # Page.
    with open(path_in_site("index.html"), "r", encoding="utf-8") as f:
        page = f.read()
    if re.search(r"\{\{[A-Z_]+\}\}", page):
        fail("index.html: unfilled template field")
    if '<esp-web-install-button manifest="manifest.json">' not in page:
        fail("index.html: no install button for manifest.json")
    m = re.search(r'<script type="importmap">(.*?)</script>', page, re.S)
    if not m:
        fail("index.html: no import map")
    imap = json.loads(m.group(1))
    csp = re.search(r'http-equiv="Content-Security-Policy" content="([^"]*)"', page)
    if not csp:
        fail("index.html: no CSP")
    if "'%s'" % sri(m.group(1).encode("utf-8")) not in csp.group(1):
        fail("index.html: CSP does not carry the hash of the import map")
    if page.index("Content-Security-Policy") > page.index('type="importmap"'):
        fail("index.html: CSP must come before the import map")
    entry = re.search(r'<script type="module" src="([^"]+)" integrity="(sha384-[A-Za-z0-9+/=]+)" crossorigin="anonymous">', page)
    if not entry:
        fail("index.html: module script without integrity")
    lock = read_json(LOCK_FILE)
    if not re.match(r"^https://cdn\.jsdelivr\.net/npm/esp-web-tools@\d+\.\d+\.\d+/dist/web/$", lock["base"]):
        fail("esp-web-tools.json: base must be an exact jsdelivr version path")
    pinned = {lock["base"] + n: h for n, h in lock["integrity"].items()}
    if pinned.get(entry.group(1)) != entry.group(2):
        fail("index.html: entry script %s not pinned in esp-web-tools.json" % entry.group(1))
    if imap.get("integrity") != {u: h for u, h in pinned.items() if u != entry.group(1)}:
        fail("index.html: import map integrity differs from esp-web-tools.json")
    if lock["base"] not in csp.group(1):
        fail("index.html: CSP script-src is not limited to the pinned path")
    for ref in re.findall(r'(?:href|src)="([^"]+)"', page):
        if not re.match(r"^(https://|#|data:)", ref):
            path_in_site(ref)
    if verify_cdn:
        for url, h in sorted(pinned.items()):
            if sri(fetch(url)) != h:
                fail("CDN: %s does not match its pinned hash" % url)
        print("  CDN: %d ESP Web Tools modules match their pinned hashes" % len(pinned))
    print("site %s OK: version %s, %d parts" % (site, manifest["version"], len(loaded)))


def cmd_check(args):
    check_site(args.site, args.verify_cdn, args.partitions_csv)


# ---------------------------------------------------------------------------------------------
# lock


def cmd_lock(args):
    if not re.match(r"^\d+\.\d+\.\d+$", args.version):
        fail("exact version x.y.z required")
    listing = json.loads(fetch(JSDELIVR_LIST.format(v=args.version)))
    base = JSDELIVR.format(v=args.version)
    integrity = {}
    for f in listing["files"]:
        if f["name"].startswith("/dist/web/") and f["name"].endswith(".js"):
            name = f["name"][len("/dist/web/"):]
            data = fetch(base + name)
            if base64.b64encode(hashlib.sha256(data).digest()).decode() != f["hash"]:
                fail("%s: download differs from the jsdelivr listing" % name)
            integrity[name] = sri(data)
    if "install-button.js" not in integrity:
        fail("no dist/web/install-button.js in esp-web-tools %s" % args.version)
    lock = {"version": args.version, "base": base, "entry": "install-button.js", "integrity": integrity}
    write_text(LOCK_FILE, json.dumps(lock, indent=2, sort_keys=True) + "\n")
    print("wrote %s: %d modules" % (LOCK_FILE, len(integrity)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("collect", help="copy the flash parts named by PlatformIO")
    p.add_argument("--metadata", required=True, help="output of pio project metadata --json-output")
    p.add_argument("--env", required=True)
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_collect)
    p = sub.add_parser("assemble", help="build the static flasher site")
    p.add_argument("--parts", required=True)
    p.add_argument("--version", required=True)
    p.add_argument("--commit", required=True)
    p.add_argument("--repo", default="dm8sb/CYD-HF-Generator")
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_assemble)
    p = sub.add_parser("check", help="validate an assembled site")
    p.add_argument("site")
    p.add_argument("--verify-cdn", action="store_true", help="download the pinned modules and compare")
    p.add_argument("--partitions-csv", help="also compare the partition table with this CSV")
    p.set_defaults(func=cmd_check)
    p = sub.add_parser("lock", help="pin an ESP Web Tools version (writes flasher/esp-web-tools.json)")
    p.add_argument("version")
    p.set_defaults(func=cmd_lock)
    args = ap.parse_args()
    try:
        args.func(args)
    except CheckError as e:
        print("error: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
