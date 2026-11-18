#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Release files and notes (ARBEITSPLAN AF23): the step of release.yml that names the assets.

Python 3 standard library only. release.yml runs it for a tag (then publishes the result) and as
a dry run on pull requests to main (then it only checks it), so the path from the build artifact
to the release assets is tested before a tag exists:

  python tools/release_files.py name --builds builds --env esp32-2432S028R --version 2.0.0 --out release
  python tools/release_files.py notes --builds builds --env esp32-2432S028R --version 2.0.0 \\
      --commit abc1234 --tag v2.0.0 --repo dm8sb/CYD-HF-Generator --pages-url https://... \\
      --packages pkg-list.md --out notes.md

name: builds/build-<env>/ is the artifact of the build job (firmware.bin, the flash parts named in
parts.json, notices/ with lgpl-sources.tar.gz). Writes cyd-hf-generator-<version>-<env>-<part>
for every part, the licenses archive (notices/ without the LGPL sources; deterministic like the
LGPL archive: sorted, mtime SOURCE_DATE_EPOCH or the commit time, owner 0, gzip without name and
time, AF25), the LGPL sources and SHA256SUMS. A missing file or part fails.

notes: the release notes with the offsets and the esptool command from parts.json and the release
file names; every part must be a file in --release (default release/). --packages appends the
resolved package list (tools/dep_check.py packages --summary).
"""
import argparse
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import flasher_site  # noqa: E402

PREFIX = "cyd-hf-generator-{version}"


class CheckError(Exception):
    pass


def _need(path):
    if not os.path.isfile(path):
        raise CheckError("missing %s" % path)
    return path


def _parts(build_dir):
    parts = flasher_site.read_json(_need(os.path.join(build_dir, "parts.json"))).get("parts") or []
    if not parts:
        raise CheckError("%s/parts.json lists no parts" % build_dir)
    for p in parts:
        name = p.get("file", "")
        if not flasher_site.NOTICE_NAME_RE.match(name) or not isinstance(p.get("offset"), int):
            raise CheckError("parts.json: bad part %r" % p)
    return parts


def _check_version(version):
    if not flasher_site.VERSION_RE.match(version):
        raise CheckError("bad version %r" % version)


def cmd_name(args):
    _check_version(args.version)
    prefix = PREFIX.format(version=args.version)
    build = os.path.join(args.builds, "build-" + args.env)
    if not os.path.isdir(build):
        raise CheckError("no build artifact %s" % build)
    os.makedirs(args.out, exist_ok=True)
    if os.listdir(args.out):
        raise CheckError("%s is not empty" % args.out)
    names = []

    def put(src, name):
        shutil.copyfile(_need(src), os.path.join(args.out, name))
        names.append(name)

    # The application of every built env, then the flash parts of the flasher env.
    for d in sorted(os.listdir(args.builds)):
        if d.startswith("build-") and d != "build-" + args.env:
            put(os.path.join(args.builds, d, "firmware.bin"), "%s-%s-firmware.bin" % (prefix, d[len("build-"):]))
    for p in _parts(build):
        put(os.path.join(build, p["file"]), "%s-%s-%s" % (prefix, args.env, p["file"]))
    if "%s-%s-firmware.bin" % (prefix, args.env) not in names:
        raise CheckError("parts.json of %s has no firmware.bin" % args.env)
    # Notices and license texts, and the LGPL sources (G11, THIRD_PARTY_NOTICES.md).
    notices = os.path.join(build, "notices")
    if not os.path.isdir(notices):
        raise CheckError("missing %s" % notices)
    lic = "%s-licenses.tar.gz" % prefix
    flasher_site.write_tar_gz(os.path.join(args.out, lic), [("notices", notices)],
                              exclude=("notices/lgpl-sources.tar.gz",))
    names.append(lic)
    put(os.path.join(notices, "lgpl-sources.tar.gz"), "%s-lgpl-sources.tar.gz" % prefix)
    sums = []
    for name in sorted(names):
        sums.append("%s  %s\n" % (flasher_site.sha256_file(os.path.join(args.out, name)), name))
    with open(os.path.join(args.out, "SHA256SUMS"), "w", encoding="utf-8", newline="\n") as f:
        f.writelines(sums)
    sys.stdout.write("".join(sums))


NOTES = """Firmware {version} (commit `{commit}`) for the ESP32-2432S028R (CYD and CYD2USB).

**Web flasher** (Chrome/Edge on a desktop): {pages}

| File | Content |
|---|---|
| `{p}-firmware.bin` | Application at 0x10000 (the build behind the web flasher) |
| `{p}-bootloader.bin`, `-partitions.bin`, `-boot_app0.bin` | Flash parts of the same build |
| `SHA256SUMS` | Checksums; provenance of every file: `gh attestation verify <file> --repo {repo}` |
| `{base}-licenses.tar.gz` | License (GPL-3.0-or-later), third-party notices and license texts |
| `{base}-lgpl-sources.tar.gz` | Source of the LGPL libraries in the firmware (Arduino-ESP32, AsyncTCP, ESPAsyncWebServer) |

Copyright (C) 2026 DM8SB. Licensed under the GNU GPL version 3 or later (GPL-3.0-or-later), WITHOUT ANY WARRANTY. Source of this release: the source archives below (tag `{tag}`); all libraries and the framework packages are pinned in `platformio.ini`. Third-party components: `THIRD_PARTY_NOTICES.md` and the licenses archive.

Offsets (PlatformIO upload):
```
{offsets}
```
Install or update with esptool 5 (does not erase NVS: touch calibration, correction and WiFi stay; esptool 4: `esptool.py` and `write_flash`):
```
esptool --chip esp32 write-flash {flash}
```
"""


def cmd_notes(args):
    _check_version(args.version)
    base = PREFIX.format(version=args.version)
    p = "%s-%s" % (base, args.env)
    parts = _parts(os.path.join(args.builds, "build-" + args.env))
    flash = []
    for part in parts:
        name = "%s-%s" % (p, part["file"])
        if not os.path.isfile(os.path.join(args.release, name)):
            raise CheckError("part %s is not a release asset" % name)
        flash.append("0x%X %s" % (part["offset"], name))
    text = NOTES.format(version=args.version, commit=args.commit, pages=args.pages_url, p=p, base=base,
                        repo=args.repo, tag=args.tag,
                        offsets="\n".join("0x%X %s" % (q["offset"], q["file"]) for q in parts),
                        flash=" ".join(flash))
    if args.packages:
        with open(args.packages, encoding="utf-8") as f:
            text += "\n<details><summary>Resolved PlatformIO packages</summary>\n\n" + f.read() + "\n</details>\n"
    with open(args.out, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print("notes in %s (%d parts)" % (args.out, len(parts)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("name", help="write the release assets and SHA256SUMS")
    p.add_argument("--builds", required=True, help="folder with build-<env>/ artifacts")
    p.add_argument("--env", required=True, help="the flasher env")
    p.add_argument("--version", required=True)
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_name)
    p = sub.add_parser("notes", help="write the release notes")
    p.add_argument("--builds", required=True)
    p.add_argument("--env", required=True)
    p.add_argument("--version", required=True)
    p.add_argument("--commit", required=True)
    p.add_argument("--tag", required=True)
    p.add_argument("--repo", required=True)
    p.add_argument("--pages-url", required=True)
    p.add_argument("--release", default="release")
    p.add_argument("--packages", help="Markdown package list to append")
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_notes)
    args = ap.parse_args()
    try:
        args.func(args)
    except (CheckError, flasher_site.CheckError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
