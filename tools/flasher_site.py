#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Web flasher site (ARBEITSPLAN G1/G2, G11): collect the flash parts and notices, assemble and check the site.

Python 3 standard library only. Used by .github/workflows/release.yml and locally:

  pio project metadata -e esp32-2432S028R --json-output > meta.json
  python tools/flasher_site.py collect --metadata meta.json --env esp32-2432S028R --out parts
  python tools/flasher_site.py notices --metadata meta.json --env esp32-2432S028R --out parts/notices
  python tools/flasher_site.py vendor --out ewt
  python tools/flasher_site.py assemble --parts parts --ewt ewt --version 2.0.0 --commit abc1234 --out site
  python tools/flasher_site.py check site
  python tools/flasher_site.py lock 10.4.0      # rewrite flasher/esp-web-tools.json (version bump)

collect: the offsets come from PlatformIO itself (FLASH_EXTRA_IMAGES of the framework build
script and the application offset of the board), not from this script. The parts stay separate:
a merged image from 0 would pad the gaps with 0xFF and erase NVS on every update.

notices (G11, THIRD_PARTY_NOTICES.md): LICENSE, LICENSES/ and THIRD_PARTY_NOTICES.md, the license
files as shipped in the pinned packages (upstream/), and the source of the LGPL parts linked into
the image (lgpl-sources.tar.gz: Arduino-ESP32 core, libraries, board variant and build scripts,
AsyncTCP, ESPAsyncWebServer, the LGPL texts and a README with the upstream tag and the ESP-IDF
commit). A missing file fails, so a dependency change cannot drop a notice unnoticed; so does a
LICENSES/TFT_eSPI-license.txt that differs from the pinned package.

vendor: ESP Web Tools is served by the flasher site itself (no CDN at runtime, E-36). Downloads
the modules pinned in flasher/esp-web-tools.json (SHA-384) and the license files and flasher stub
sources listed under "notices" there (SHA-256); a file that differs from its pin fails. Files
already in --out with the right hash are not downloaded again.

check: manifest and files, every part an ESP32 image or the partition table where expected,
offsets sector-aligned, ascending and non-overlapping (erase granularity 4 KiB), every part inside
the bootloader area, the partition table sector or an app/otadata partition of the flashed
partition table (so an update never erases NVS), checksums, page template fully filled, the
self-hosted ESP Web Tools modules and notices against their pins, a CSP that allows scripts only
from the site itself, no resource from another origin, and every local link present.
"""
import argparse
import base64
import hashlib
import html
import io
import json
import os
import re
import shutil
import struct
import sys
import tarfile
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
EWT_SITE_DIR = "esp-web-tools/{v}/"   # the modules in the site (relative imports keep working)
EWT_NOTICES_DIR = "notices/esp-web-tools/"
NOTICE_NAME_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,99}$")
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
# notices


def _package_dirs(meta, env):
    """Arduino-ESP32 framework and toolchain directories of this build, from the PlatformIO metadata."""
    extra = meta.get("extra") or {}
    framework = None
    for i in extra.get("flash_images", []):
        parts = os.path.normpath(i["path"]).split(os.sep)
        if parts[-3:] == ["tools", "partitions", "boot_app0.bin"]:
            framework = os.sep.join(parts[:-3])
    if not framework or not os.path.isfile(os.path.join(framework, "package.json")):
        fail("metadata of %s names no Arduino-ESP32 framework (boot_app0.bin)" % env)
    toolchain = os.path.dirname(os.path.dirname(meta.get("cc_path", "")))
    if not os.path.isdir(os.path.join(toolchain, "share", "licenses")):
        fail("metadata of %s: no toolchain licenses next to %s" % (env, meta.get("cc_path")))
    return framework, toolchain


def _head(path, lines):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = "".join(f.readline() for _ in range(lines))
    if not text.strip():
        fail("%s: empty header" % path)
    return text


def _need(path):
    if not os.path.exists(path):
        fail("missing %s" % path)
    return path


def cmd_notices(args):
    meta = read_json(args.metadata)
    if args.env not in meta:
        fail("env %s not in %s" % (args.env, args.metadata))
    framework, toolchain = _package_dirs(meta[args.env], args.env)
    lib = args.libdeps or os.path.join(ROOT, ".pio", "libdeps", args.env)
    out = args.out
    up = os.path.join(out, "upstream")
    os.makedirs(up, exist_ok=True)

    shutil.copyfile(_need(os.path.join(ROOT, "THIRD_PARTY_NOTICES.md")), os.path.join(out, "THIRD_PARTY_NOTICES.txt"))
    shutil.copyfile(_need(os.path.join(ROOT, "LICENSE")), os.path.join(out, "LICENSE.txt"))
    shutil.copyfile(_need(os.path.join(ROOT, "LICENSE-EXCEPTION.md")), os.path.join(out, "LICENSE-EXCEPTION.txt"))
    shutil.copytree(_need(os.path.join(ROOT, "LICENSES")), os.path.join(out, "LICENSES"), dirs_exist_ok=True)
    # License files exactly as shipped (the file header where a package has no license file).
    lic = os.path.join(toolchain, "share", "licenses")
    copies = [
        ("TFT_eSPI-license.txt", os.path.join(lib, "TFT_eSPI", "license.txt")),
        ("lvgl-LICENCE.txt", os.path.join(lib, "lvgl", "LICENCE.txt")),
        ("scpi-parser-LICENSE.txt", os.path.join(lib, "scpi-parser", "LICENSE")),
        ("ArduinoJson-LICENSE.txt", os.path.join(lib, "ArduinoJson", "LICENSE.txt")),
        ("AsyncTCP-LICENSE.txt", os.path.join(lib, "AsyncTCP", "LICENSE")),
        ("ESPAsyncWebServer-LICENSE.txt", os.path.join(lib, "ESPAsyncWebServer", "LICENSE")),
        ("libb64-LICENSE.txt", os.path.join(framework, "cores", "esp32", "libb64", "LICENSE")),
        ("newlib-COPYING.NEWLIB.txt", os.path.join(lic, "newlib", "COPYING.NEWLIB")),
        ("gcc-COPYING.RUNTIME.txt", os.path.join(lic, "gcc", "COPYING.RUNTIME")),
    ]
    for name, src in copies:
        shutil.copyfile(_need(src), os.path.join(up, name))
    # The TFT_eSPI license is also in the repository (display_detect.cpp copies its ST7789 init
    # sequence, LIC-7); a version bump that changes it must update LICENSES/ as well.
    if _read(_need(os.path.join(ROOT, "LICENSES", "TFT_eSPI-license.txt"))) != _read(os.path.join(up, "TFT_eSPI-license.txt")):
        fail("LICENSES/TFT_eSPI-license.txt differs from the license.txt of the pinned TFT_eSPI")
    lvgl =os.path.join(lib, "lvgl", "src")
    headers = [
        ("lvgl-qrcodegen-header.txt", os.path.join(lvgl, "extra", "libs", "qrcode", "qrcodegen.c"), 25),
        ("lvgl-tlsf-header.txt", os.path.join(lvgl, "misc", "lv_tlsf.h"), 45),
        ("lvgl-printf-header.txt", os.path.join(lvgl, "misc", "lv_printf.c"), 30),
        ("XPT2046_Touchscreen-header.txt", os.path.join(lib, "XPT2046_Touchscreen", "XPT2046_Touchscreen.cpp"), 21),
        ("ESPmDNS-header.txt", os.path.join(framework, "libraries", "ESPmDNS", "src", "ESPmDNS.cpp"), 30),
    ]
    for name, src, lines in headers:
        write_text(os.path.join(up, name), _head(_need(src), lines))

    # Complete source of the LGPL parts linked into the image (LGPL-2.1 section 6, LGPL-3.0
    # section 4): the Arduino-ESP32 core and libraries with the board variant and the build
    # scripts that compile them, AsyncTCP, ESPAsyncWebServer, the license texts and a README
    # naming the upstream versions (AF29, LIC-5).
    def skip(info):
        return None if info.name.rsplit("/", 1)[-1] in (".git", ".piopm", "__pycache__") else info

    readme = _lgpl_readme(framework, _idf_versions(meta[args.env]), lib).encode("utf-8")
    tar_path = os.path.join(out, "lgpl-sources.tar.gz")
    with tarfile.open(tar_path, "w:gz") as t:
        info = tarfile.TarInfo("README.txt")
        info.size = len(readme)
        info.mode = 0o644
        t.addfile(info, io.BytesIO(readme))
        for rel in LGPL_FRAMEWORK_PARTS:
            t.add(_need(os.path.join(framework, *rel.split("/"))),
                  arcname="framework-arduinoespressif32/" + rel, filter=skip)
        for name in ("AsyncTCP", "ESPAsyncWebServer"):
            t.add(_need(os.path.join(lib, name)), arcname=name, filter=skip)
        for arc, src in (("LICENSES/LGPL-2.1-or-later.txt", os.path.join(ROOT, "LICENSES", "LGPL-2.1-or-later.txt")),
                         ("LICENSES/LGPL-3.0-or-later.txt", os.path.join(ROOT, "LICENSES", "LGPL-3.0-or-later.txt")),
                         ("LICENSES/GPL-3.0.txt", os.path.join(ROOT, "LICENSE"))):
            t.add(_need(src), arcname=arc)
    print("notices in %s: %d upstream files, lgpl-sources.tar.gz %d B (framework %s)"
          % (out, len(copies) + len(headers), os.path.getsize(tar_path), framework))


# Parts of the Arduino-ESP32 package in lgpl-sources.tar.gz: the core and libraries, the board
# variant of esp32dev, the partition tables and the build scripts that compile them (AF29, LIC-5).
LGPL_FRAMEWORK_PARTS = ("package.json", "cores", "libraries", "variants/esp32", "tools/partitions",
                        "tools/platformio-build.py", "tools/pioarduino-build.py")


def _idf_versions(meta):
    """versions.txt of the precompiled ESP-IDF libraries of this build (found via the include
    paths): {"esp-idf": "v5.5.5 b774170ff46", ...} and the package directory."""
    seen = set()
    for path in (meta.get("includes") or {}).get("build", []):
        d = os.path.normpath(path)
        for _ in range(4):
            d = os.path.dirname(d)
            if d in seen:
                break
            seen.add(d)
            vt = os.path.join(d, "versions.txt")
            if os.path.isfile(vt) and os.path.isfile(os.path.join(d, "package.json")):
                versions = {}
                with open(vt, "r", encoding="utf-8") as f:
                    for line in f:
                        k, _, v = line.partition(":")
                        versions[k.strip()] = v.strip()
                if "esp-idf" in versions:
                    return versions, d
    fail("metadata names no precompiled ESP-IDF libraries with versions.txt")


def _lgpl_readme(framework, idf, lib):
    versions, libs_dir = idf
    with open(os.path.join(framework, "cores", "esp32", "esp_arduino_version.h"), "r", encoding="utf-8") as f:
        text = f.read()
    v = [re.search(r"#define ESP_ARDUINO_VERSION_%s\s+(\d+)" % k, text) for k in ("MAJOR", "MINOR", "PATCH")]
    if not all(v):
        fail("%s: no ESP_ARDUINO_VERSION" % framework)
    arduino = ".".join(m.group(1) for m in v)
    pkg = read_json(os.path.join(framework, "package.json"))
    libs_pkg = read_json(os.path.join(libs_dir, "package.json"))
    libver = {}
    for name in ("AsyncTCP", "ESPAsyncWebServer"):
        libver[name] = read_json(_need(os.path.join(lib, name, "library.json")))["version"]
    return """LGPL sources of the CYD HF Generator firmware
=============================================

Complete source of the libraries under the GNU Lesser General Public License that are linked
into the firmware image (LGPL-2.1 section 6, LGPL-3.0 section 4). The source of the firmware
itself (GPL-3.0-or-later) is the tagged commit of https://github.com/dm8sb/CYD-HF-Generator,
whose platformio.ini pins these versions.

framework-arduinoespressif32/   Arduino-ESP32 {arduino} (LGPL-2.1-or-later, some files Apache-2.0
                                or MIT, see each header), PlatformIO package
                                framework-arduinoespressif32 {pkgver}
                                Upstream tag: https://github.com/espressif/arduino-esp32/tree/{arduino}
                                Contents: {parts}
AsyncTCP/                       AsyncTCP {asynctcp} (LGPL-3.0-or-later), https://github.com/ESP32Async/AsyncTCP
ESPAsyncWebServer/              ESPAsyncWebServer {aws} (LGPL-3.0-or-later), https://github.com/ESP32Async/ESPAsyncWebServer
LICENSES/                       LGPL-2.1, LGPL-3.0 and the GNU GPL 3.0 the LGPL-3.0 refers to

Not in this archive (not under the LGPL): the precompiled ESP-IDF libraries the Arduino core is
built against, PlatformIO package {libs_name} {libs_ver} (Apache-2.0 and the
licenses in THIRD_PARTY_NOTICES.md), built by esp32-arduino-lib-builder {builder} from
ESP-IDF {idf} (https://github.com/espressif/esp-idf/tree/{idf_tag}).

Relinking against a modified library: check out the tagged commit, replace the library (for
example a copy of framework-arduinoespressif32 via platform_packages, or AsyncTCP via lib_deps
or the lib/ folder) and run `pio run -e esp32-2432S028R`.
""".format(arduino=arduino, pkgver=pkg.get("version", "?"), parts=", ".join(LGPL_FRAMEWORK_PARTS),
           asynctcp=libver["AsyncTCP"], aws=libver["ESPAsyncWebServer"],
           libs_name=libs_pkg.get("name", "?"), libs_ver=libs_pkg.get("version", "?"),
           builder=versions.get("lib-builder", "?"), idf=versions["esp-idf"],
           idf_tag=versions["esp-idf"].split()[0])


# ---------------------------------------------------------------------------------------------
# vendor (self-hosted ESP Web Tools)


def _lock_entries(lock):
    """(kind, name, url, pin) of every pinned file: the modules (SHA-384) and the notices (SHA-256)."""
    for name, h in sorted(lock["integrity"].items()):
        yield "web", name, lock["base"] + name, h
    for name, n in sorted(lock.get("notices", {}).items()):
        if not NOTICE_NAME_RE.match(name):
            fail("esp-web-tools.json: bad notice name %r" % name)
        yield "notices", name, n["url"], n["sha256"]


def _matches(data, pin):
    if pin.startswith("sha384-"):
        return sri(data) == pin
    return hashlib.sha256(data).hexdigest() == pin


def _read(path):
    with open(path, "rb") as f:
        return f.read()


def cmd_vendor(args):
    lock = read_json(LOCK_FILE)
    fetched = cached = 0
    for kind, name, url, pin in _lock_entries(lock):
        if not url.startswith("https://"):
            fail("%s: only https downloads" % name)
        path = os.path.join(args.out, kind, name)
        if os.path.isfile(path) and _matches(_read(path), pin):
            cached += 1
            continue
        data = fetch(url)
        if not _matches(data, pin):
            fail("%s: download from %s does not match its pin in esp-web-tools.json" % (name, url))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
        fetched += 1
    print("ESP Web Tools %s in %s: %d files downloaded, %d cached, all match their pins"
          % (lock["version"], args.out, fetched, cached))


# ---------------------------------------------------------------------------------------------
# assemble


def _source_links(repo, version, commit):
    """Source and release links of the page (AF30, E-39): a tag build (version 2.0.0 or
    2.0.0-rc1, as the release workflow derives it from the tag v2.0.0 / v2.0.0-rc1) links its
    tag and release; a manual build (2.0.0-dev.<commit>) has no release and links the commit."""
    if not re.match(r"^[0-9a-f]{7,40}$", commit):
        fail("commit %r: 7 to 40 hex digits expected" % commit)
    base = "https://github.com/" + repo
    if "-dev." in version:
        ref, release_url, release_name = commit, base + "/releases", "releases (none for this test build)"
    else:
        ref = "v" + version
        release_url, release_name = base + "/releases/tag/" + ref, "release " + ref
    return {"SOURCE_URL": html.escape(base + "/tree/" + ref), "SOURCE_REF": html.escape(ref),
            "RELEASE_URL": html.escape(release_url), "RELEASE_NAME": html.escape(release_name)}


def cmd_assemble(args):
    if not VERSION_RE.match(args.version):
        fail("version %r: only letters, digits and . + - allowed" % args.version)
    lock = read_json(LOCK_FILE)
    info = read_json(os.path.join(args.parts, "parts.json"))
    notices = os.path.join(args.parts, "notices")
    if not os.path.isfile(os.path.join(notices, "THIRD_PARTY_NOTICES.txt")):
        fail("%s: no notices/ (run the notices command first)" % args.parts)
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

    # Notices and LGPL sources of the same build; ESP Web Tools and its notices from the vendor
    # directory, checked against their pins once more.
    shutil.copytree(notices, os.path.join(args.out, "notices"), dirs_exist_ok=True)
    ewt_rel = EWT_SITE_DIR.format(v=lock["version"])
    ewt_links = []
    for kind, name, url, pin in _lock_entries(lock):
        src = os.path.join(args.ewt, kind, name)
        if not os.path.isfile(src) or not _matches(_read(src), pin):
            fail("%s: missing in %s or not matching its pin (run vendor)" % (name, args.ewt))
        rel = (ewt_rel if kind == "web" else EWT_NOTICES_DIR) + name
        dst = os.path.join(args.out, *rel.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(src, dst)
        if kind == "notices":
            ewt_links.append('<li><a href="%s">%s</a></li>' % (html.escape(rel), html.escape(name)))

    with open(os.path.join(TEMPLATE_DIR, "index.html"), "r", encoding="utf-8") as f:
        page = f.read()
    fields = {
        "EWT_PATH": html.escape(ewt_rel),
        "EWT_ENTRY": html.escape(lock["entry"]),
        "EWT_ENTRY_INTEGRITY": lock["integrity"][lock["entry"]],
        "EWT_VERSION": html.escape(lock["version"]),
        "EWT_NOTICES": "\n".join(ewt_links),
        "VERSION": html.escape(args.version),
        "COMMIT": html.escape(args.commit),
        "PARTS_ROWS": "\n".join(rows),
        "SUMS_PATH": fw_rel + "/SHA256SUMS",
        "REPO_NAME": html.escape(args.repo),
    }
    fields.update(_source_links(args.repo, args.version, args.commit))
    page = re.sub(r"\{\{([A-Z_]+)\}\}", lambda m: fields[m.group(1)], page)
    write_text(os.path.join(args.out, "index.html"), page)
    shutil.copyfile(os.path.join(TEMPLATE_DIR, "style.css"), os.path.join(args.out, "style.css"))
    print("site assembled in %s (version %s, %d parts, ESP Web Tools %s self-hosted)"
          % (args.out, args.version, len(rows), lock["version"]))


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


def check_site(site, partitions_csv=None):
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
    csp = re.search(r'http-equiv="Content-Security-Policy" content="([^"]*)"', page)
    if not csp:
        fail("index.html: no CSP")
    directives = {d.split()[0]: d.split()[1:] for d in csp.group(1).split(";") if d.strip()}
    if directives.get("script-src") != ["'self'"]:
        fail("index.html: CSP script-src must be 'self' only (ESP Web Tools is self-hosted)")
    if directives.get("default-src") != ["'none'"]:
        fail("index.html: CSP default-src must be 'none'")
    if re.search(r"https?:|\*", csp.group(1)):
        fail("index.html: CSP allows another origin")
    if page.index("Content-Security-Policy") > page.lower().index("<script"):
        fail("index.html: CSP must come before the first script")
    # Scripts: only the pinned entry module, no inline code. Case-insensitive and also
    # "</script >" (HTML allows both), so no script slips past the check (CodeQL py/bad-tag-filter).
    scripts = re.findall(r"<script\b([^>]*)>(.*?)</script\s*>", page, re.S | re.I)
    lock = read_json(LOCK_FILE)
    ewt_rel = EWT_SITE_DIR.format(v=lock["version"])
    entry_src = ewt_rel + lock["entry"]
    want = ' type="module" src="%s" integrity="%s"' % (entry_src, lock["integrity"][lock["entry"]])
    if [s for s in scripts] != [(want, "")]:
        fail("index.html: expected exactly <script%s></script>, got %s" % (want, scripts))
    # Resources (src, link href) only from the site; links to other sites are plain <a>.
    for tag, attr, ref in re.findall(r'<(\w+)\b[^>]*?\b(href|src)="([^"]*)"', page):
        if tag.lower() == "a" and re.match(r"^(https://|#)", ref):
            continue
        if ref.startswith("data:") and tag.lower() == "link":
            continue
        path_in_site(ref)
    # ESP Web Tools and its notices as pinned (the page links each notice).
    for kind, name, url, pin in _lock_entries(lock):
        rel = (ewt_rel if kind == "web" else EWT_NOTICES_DIR) + name
        if not _matches(_read(path_in_site(rel)), pin):
            fail("%s does not match its pin in esp-web-tools.json" % rel)
        if kind == "notices" and 'href="%s"' % rel not in page:
            fail("index.html: no link to %s" % rel)
    # Firmware notices (G11): the license texts and the LGPL sources must be linked and present.
    for rel in ("notices/LICENSE.txt", "notices/LICENSE-EXCEPTION.txt", "notices/THIRD_PARTY_NOTICES.txt",
                "notices/lgpl-sources.tar.gz"):
        path_in_site(rel)
        if 'href="%s"' % rel not in page:
            fail("index.html: no link to %s" % rel)
    # Source links point to the tag or commit of this build, not to a moving branch (E-39).
    if not re.search(r'href="https://github\.com/[^"/]+/[^"/]+/tree/(v[0-9A-Za-z.+-]+|[0-9a-f]{7,40})"', page):
        fail("index.html: no source link to a tag or commit")
    print("  ESP Web Tools %s self-hosted: %d modules and %d notices match their pins"
          % (lock["version"], len(lock["integrity"]), len(lock.get("notices", {}))))
    print("site %s OK: version %s, %d parts" % (site, manifest["version"], len(loaded)))


def cmd_check(args):
    check_site(args.site, args.partitions_csv)


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
    old = read_json(LOCK_FILE) if os.path.isfile(LOCK_FILE) else {}
    lock = {"version": args.version, "base": base, "entry": "install-button.js", "integrity": integrity,
            "notices": old.get("notices", {})}
    write_text(LOCK_FILE, json.dumps(lock, indent=2, sort_keys=True) + "\n")
    print("wrote %s: %d modules" % (LOCK_FILE, len(integrity)))
    if old.get("version") != args.version:
        print("note: \"notices\" kept from %s; check the bundled packages and flasher stubs of %s "
              "and update them and THIRD_PARTY_NOTICES.md" % (old.get("version"), args.version))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("collect", help="copy the flash parts named by PlatformIO")
    p.add_argument("--metadata", required=True, help="output of pio project metadata --json-output")
    p.add_argument("--env", required=True)
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_collect)
    p = sub.add_parser("notices", help="collect the license notices and LGPL sources of a build")
    p.add_argument("--metadata", required=True, help="output of pio project metadata --json-output")
    p.add_argument("--env", required=True)
    p.add_argument("--libdeps", help="library directory (default .pio/libdeps/<env>)")
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_notices)
    p = sub.add_parser("vendor", help="download the pinned ESP Web Tools files and notices")
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_vendor)
    p = sub.add_parser("assemble", help="build the static flasher site")
    p.add_argument("--parts", required=True, help="output of collect, with notices/ from the notices command")
    p.add_argument("--ewt", required=True, help="output of vendor")
    p.add_argument("--version", required=True)
    p.add_argument("--commit", required=True)
    p.add_argument("--repo", default="dm8sb/CYD-HF-Generator")
    p.add_argument("--out", required=True)
    p.set_defaults(func=cmd_assemble)
    p = sub.add_parser("check", help="validate an assembled site")
    p.add_argument("site")
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
