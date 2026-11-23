#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Pinned dependencies of the firmware and the flasher (ARBEITSPLAN AF22, AF26): pins and advisories.

Python 3 standard library only. Used by .github/workflows/ci.yml and release.yml and locally:

  pio pkg list -e esp32-2432S028R > pkg-list.txt        # PYTHONIOENCODING=utf-8 on Windows
  pio project metadata -e esp32-2432S028R --json-output > meta.json
  python tools/dep_check.py packages --pkg-list pkg-list.txt --metadata meta.json --env esp32-2432S028R

packages: the platform must be one exact release asset URL and resolve to it; every package of
`platform_packages` (by URL or version) and every registry library of `lib_deps` in platformio.ini
must resolve to exactly its pin; the framework packages must be pinned there, the toolchain and
esptool must resolve to the versions the pinned platform names (VIA_PLATFORM); every URL pin must
match the SHA-256 digest GitHub publishes for the asset (ASSET_SHA256, E-804; token from
GITHUB_TOKEN/GH_TOKEN); and the versions THIRD_PARTY_NOTICES.md states (Arduino-ESP32, the ESP-IDF tag and
commit of the precompiled libraries, the GCC runtime, the libraries) must match what the build
uses. --summary appends the resolved list as Markdown (job summary, release notes).

advisories (AF26, A-724): the pinned versions against the published security advisories of
their upstream repositories (GitHub repository advisories: the libraries of lib_deps, the version
of git pins from their newest contained tag, Arduino-ESP32 and ESP-IDF as THIRD_PARTY_NOTICES.md
names them, ESP Web Tools and esptool-js) and against OSV (the npm packages of
flasher/esp-web-tools.json, the commits of the git pins). Advisories that do not apply are
recorded with their reason in tools/advisories_accepted.json. A finding (affected, or a version
range that cannot be read) fails; --issue-body writes the report for an issue. Token: GITHUB_TOKEN
or GH_TOKEN. --override name=version checks another version instead (test with a known old one):

  python tools/dep_check.py advisories --override ESPAsyncWebServer=3.11.0
"""
import argparse
import configparser
import json
import os
import re
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import flasher_site  # noqa: E402  (framework directory and ESP-IDF versions of a build)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INI = os.path.join(ROOT, "platformio.ini")
NOTICES = os.path.join(ROOT, "THIRD_PARTY_NOTICES.md")
# The packages that compile, link and convert the firmware; each must be pinned exactly (AF22).
MUST_PIN = ("framework-arduinoespressif32", "framework-arduinoespressif32-libs")
# The toolchain and esptool cannot be pinned in platform_packages: pioarduino installs them with
# ESP-IDF's idf_tools.py (checksums from its tools.json) and a platform_packages entry hides them
# from the build (compiler not found). The platform names them by exact URL, and the platform
# archive is pinned by its digest below, so they must resolve exactly to these versions.
VIA_PLATFORM = {"toolchain-xtensa-esp-elf": "14.4.0+20261030", "tool-esptoolpy": "5.6.0"}
# The pioarduino platform and its packages are GitHub release assets pinned by URL (E-804); PlatformIO
# checks no checksum for them, so every pinned URL must be listed here with the SHA-256 digest GitHub
# publishes for the asset (`packages` compares it over the API: a replaced asset fails).
ASSET_SHA256 = {
    "https://github.com/pioarduino/platform-espressif32/releases/download/55.03.313/platform-espressif32.zip":
        "ff81417ffa1167113517dcf0a6a957eda683a7ad66dc91f8bffb151b4d554193",
    "https://github.com/espressif/arduino-esp32/releases/download/3.3.13/esp32-core-3.3.13.tar.xz":
        "80c3e459f715fed3265e2c62cfc376216e1cc3fc75dce158636aec0ba5a1f0e6",
    "https://github.com/espressif/arduino-esp32/releases/download/3.3.13/esp32-core-3.3.13-libs.tar.xz":
        "98c080079485bbbd5147541de4f0750f7e565252b71d1dc1fb5281a4e11d7144",
}
RELEASE_ASSET = re.compile(r"^https://github\.com/([\w.-]+/[\w.-]+)/releases/download/([^/]+)/([^/]+)$")
PLATFORM_LINE = re.compile(r"^Platform (\S+) @ (\S+) \(required: (.+)\)\s*$", re.M)
PKG_LINE = re.compile(r"^[\s│]*[├└]── (\S+) @ (\S+) \(required: (.+)\)\s*$")


class CheckError(Exception):
    pass


def ini_list(section, key):
    """The lines of a multi-line platformio.ini value, without comments and empty lines."""
    cp = configparser.ConfigParser(interpolation=None, inline_comment_prefixes=(";",))
    cp.read(INI, encoding="utf-8")
    if not cp.has_option(section, key):
        return []
    out = []
    for line in cp.get(section, key).splitlines():
        line = line.split(";", 1)[0].strip()
        if line:
            out.append(line)
    return out


def parse_spec(spec):
    """'owner/name @ 1.2.3' or 'owner/name@1.2.3' -> (name, version), 'name @ https://...' ->
    (name, url); None for git URLs without a package name (lib_deps)."""
    m = re.match(r"^(?:[\w.-]+/)?([\w.-]+)\s*@\s*(https://\S+)$", spec.strip())
    if m:
        return m.group(1), m.group(2)
    if "://" in spec:
        return None
    m = re.match(r"^(?:[\w.-]+/)?([\w.-]+)\s*@\s*(\S+)$", spec.strip())
    return (m.group(1), m.group(2)) if m else None


def parse_pkg_list(text):
    """{name: (version, required)} of every line of `pio pkg list`."""
    pkgs = {}
    for line in text.splitlines():
        m = PKG_LINE.match(line)
        if m:
            pkgs[m.group(1)] = (m.group(2), m.group(3))
    if not pkgs:
        raise CheckError("no packages in the pio pkg list output")
    return pkgs


def notice_versions(text):
    """The versions THIRD_PARTY_NOTICES.md states, from the first columns of its table rows."""
    rows = {}
    for line in text.splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 2 and line.lstrip().startswith("|"):
            rows.setdefault(cells[0], cells[1])

    def row(prefix):
        for k, v in rows.items():
            if k.startswith(prefix):
                return v
        raise CheckError("THIRD_PARTY_NOTICES.md: no row %r" % prefix)

    return {
        "arduino": row("Arduino-ESP32 core and libraries"),
        "esp-idf": row("ESP-IDF (system"),
        "gcc": row("GCC runtime"),
        "TFT_eSPI": row("TFT_eSPI"),
        "lvgl": row("LVGL"),
        "ArduinoJson": row("ArduinoJson"),
        "AsyncTCP": row("AsyncTCP"),
        "ESPAsyncWebServer": row("ESPAsyncWebServer"),
        "XPT2046_Touchscreen": row("XPT2046_Touchscreen"),
        "scpi-parser": row("scpi-parser"),
    }


def arduino_version(framework):
    with open(os.path.join(framework, "cores", "esp32", "esp_arduino_version.h"), encoding="utf-8") as f:
        text = f.read()
    parts = [re.search(r"#define ESP_ARDUINO_VERSION_%s\s+(\d+)" % k, text) for k in ("MAJOR", "MINOR", "PATCH")]
    if not all(parts):
        raise CheckError("%s: no ESP_ARDUINO_VERSION" % framework)
    return ".".join(m.group(1) for m in parts)


def asset_digest(url, token):
    """SHA-256 digest GitHub publishes for a release asset URL, or None if the asset has none."""
    m = RELEASE_ASSET.match(url)
    if not m:
        raise CheckError("%s is not a GitHub release asset" % url)
    repo, tag, name = m.groups()
    rel = _http_json("%s/repos/%s/releases/tags/%s" % (GITHUB_API, repo, tag), token=token)
    for a in rel.get("assets", []):
        if a.get("name") == name:
            d = a.get("digest") or ""
            return d.split(":", 1)[1] if d.startswith("sha256:") else None
    raise CheckError("%s: no asset %s in release %s" % (repo, name, tag))


def check_url_pins(urls, problems):
    """Every URL pin must be listed in ASSET_SHA256 and match the digest GitHub publishes."""
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    for url in urls:
        want = ASSET_SHA256.get(url)
        if want is None:
            problems.append("%s has no SHA-256 in ASSET_SHA256 (tools/dep_check.py)" % url)
            continue
        got = asset_digest(url, token)
        if got != want:
            problems.append("%s: GitHub digest %s, pinned %s" % (url, got, want))


def idf_expectation(ref, commit, note):
    """(wanted text, matches) for the ESP-IDF row. A tag build ('v5.5.5') must be named as
    'v5.5.5 (`commit`)'; a branch build ('release/v5.5', the pioarduino libraries, E-804) as
    'v5.5.6 (`commit`, branch release/v5.5)' with the newest release tag the commit contains, which
    `advisories` checks against GitHub and uses for the advisories."""
    if parse_version(ref):
        want = "%s (`%s`)" % (ref, commit)
        return want, note == want
    want = "vX.Y.Z (`%s`, branch %s)" % (commit, ref)
    m = re.match(r"^v\d+\.\d+\.\d+ \(`([0-9a-f]+)`, branch (\S+)\)$", note)
    return want, bool(m) and m.group(1) == commit and m.group(2) == ref


def cmd_packages(args):
    with open(args.pkg_list, encoding="utf-8") as f:
        text = f.read()
    pkgs = parse_pkg_list(text)
    problems = []
    # The platform: one exact release asset (E-804), never a registry range or a branch.
    platform = ini_list("esp32", "platform")
    platform = platform[0] if platform else ""
    m = RELEASE_ASSET.match(platform)
    if not m:
        problems.append("platform %r is not pinned to a release asset URL" % platform)
    url_pins = [platform] if m else []
    resolved = PLATFORM_LINE.search(text)
    if not resolved or resolved.group(3).strip() != platform:
        problems.append("the build resolved platform %s, pinned %s"
                        % (resolved.group(3).strip() if resolved else "nothing", platform))
    elif m and resolved.group(2) != ".".join(str(int(x)) for x in m.group(2).split(".")):
        problems.append("platform %s reports version %s" % (m.group(2), resolved.group(2)))
    pinned = {}
    for spec in ini_list("esp32", "platform_packages"):
        p = parse_spec(spec)
        if not p:
            problems.append("platform_packages: cannot read %r" % spec)
            continue
        pinned[p[0]] = p[1]
    for name in MUST_PIN:
        if name not in pinned:
            problems.append("%s is not pinned exactly in platform_packages" % name)
    for name, want in sorted(VIA_PLATFORM.items()):
        got = pkgs.get(name)
        if name in pinned:
            problems.append("%s must not be in platform_packages (installed by the platform)" % name)
        elif got is None or got[0] != want:
            problems.append("%s resolved to %s, the platform pins %s" % (name, got[0] if got else "nothing", want))
    libs = {}
    for spec in ini_list("esp32", "lib_deps"):
        p = parse_spec(spec)
        if p:
            libs[p[0]] = p[1]
    for name, want in sorted(list(pinned.items()) + list(libs.items())):
        got = pkgs.get(name)
        if got is None:
            problems.append("%s @ %s is pinned but not in the resolved packages" % (name, want))
        elif "://" in want:
            url_pins.append(want)
            if want not in got[1]:
                problems.append("%s resolved from %s, pinned %s" % (name, got[1], want))
        elif got[0] != want:
            problems.append("%s resolved to %s, pinned %s" % (name, got[0], want))
    try:
        check_url_pins(url_pins, problems)
    except OSError as e:
        problems.append("GitHub release digests not readable: %s" % e)

    # THIRD_PARTY_NOTICES.md against the build (framework and ESP-IDF from the metadata).
    meta = flasher_site.read_json(args.metadata)
    if args.env not in meta:
        raise CheckError("env %s not in %s" % (args.env, args.metadata))
    try:
        framework, _ = flasher_site._package_dirs(meta[args.env], args.env)
        idf, _ = flasher_site._idf_versions(meta[args.env])
    except flasher_site.CheckError as e:
        raise CheckError(str(e))
    with open(NOTICES, encoding="utf-8") as f:
        notes = notice_versions(f.read())
    arduino = arduino_version(framework)
    idf_tag, _, idf_commit = idf["esp-idf"].partition(" ")
    tc = pkgs.get("toolchain-xtensa-esp-elf", ("?", ""))[0]
    m = re.match(r"^(\d+\.\d+\.\d+)\+(\d+)$", tc)
    gcc = "%s (esp-%s_%s)" % (m.group(1), m.group(1), m.group(2)) if m else "?"
    expect = [
        ("arduino", arduino, notes["arduino"] == arduino),
        ("esp-idf",) + idf_expectation(idf_tag, idf_commit[:11], notes["esp-idf"]),
        ("gcc", gcc, notes["gcc"] == gcc),
    ]
    for name in ("TFT_eSPI", "lvgl", "ArduinoJson", "AsyncTCP", "ESPAsyncWebServer"):
        v = pkgs.get(name, ("?", ""))[0]
        expect.append((name, v, notes[name] == v))
    for name in ("XPT2046_Touchscreen", "scpi-parser"):
        v = pkgs.get(name, ("?", ""))[0]
        sha = v.rsplit("sha.", 1)[-1] if "sha." in v else "?"
        expect.append((name, "commit `%s`" % sha, ("`%s`" % sha) in notes[name]))
    for name, want, ok in expect:
        if not ok:
            problems.append("THIRD_PARTY_NOTICES.md: %s says %r, the build uses %s" % (name, notes[name], want))

    lines = ["### Resolved packages (%s)" % args.env, "", "| Package | Version | Required |", "|---|---|---|"]
    lines += ["| %s | %s | `%s` |" % (n, v, r) for n, (v, r) in sorted(pkgs.items())]
    lines += ["", "Arduino-ESP32 %s, ESP-IDF %s, GCC %s." % (arduino, idf["esp-idf"], gcc), ""]
    text = "\n".join(lines) + "\n"
    if args.summary:
        with open(args.summary, "a", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    if problems:
        raise CheckError("\n  ".join(["pinned packages differ:"] + problems))
    print("packages OK: %d resolved, %d pinned exactly in platform_packages, notices match"
          % (len(pkgs), len(pinned)))


# ---------------------------------------------------------------------------------------------
# advisories (AF26)

# GitHub repository of every registry library in lib_deps (a new library must be added here).
LIB_REPOS = {
    "TFT_eSPI": "Bodmer/TFT_eSPI",
    "lvgl": "lvgl/lvgl",
    "ArduinoJson": "bblanchon/ArduinoJson",
    "AsyncTCP": "ESP32Async/AsyncTCP",
    "ESPAsyncWebServer": "ESP32Async/ESPAsyncWebServer",
}
# npm packages of the flasher whose upstream repository also publishes GitHub advisories.
NPM_REPOS = {"esp-web-tools": "esphome/esp-web-tools", "esptool-js": "espressif/esptool-js"}
LOCK_FILE = os.path.join(ROOT, "flasher", "esp-web-tools.json")
ACCEPTED_FILE = os.path.join(ROOT, "tools", "advisories_accepted.json")
GITHUB_API = "https://api.github.com"
OSV_API = "https://api.osv.dev/v1"
VERSION_TOKEN = re.compile(r"^v?(\d+(?:\.\d+)*)(?:[-+.]?([0-9A-Za-z][0-9A-Za-z.-]*))?$")


def _http_json(url, data=None, token=None):
    headers = {"User-Agent": "cyd-dep-check", "Accept": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
        headers["Accept"] = "application/vnd.github+json"
    body = None
    if data is not None:
        body = json.dumps(data).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=body, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as r:
        return json.loads(r.read().decode("utf-8"))


def parse_version(text):
    """'v5.5.5' -> ((5, 5, 5), ''), '4.0.0-alpha1' -> ((4, 0, 0), 'alpha1'); None if not a version."""
    m = VERSION_TOKEN.match(text.strip())
    if not m:
        return None
    nums = tuple(int(x) for x in m.group(1).split("."))
    return nums, (m.group(2) or "")


def _cmp(a, b):
    """Compare two parsed versions; a pre-release sorts before its release."""
    n = max(len(a[0]), len(b[0]))
    x = a[0] + (0,) * (n - len(a[0]))
    y = b[0] + (0,) * (n - len(b[0]))
    if x != y:
        return -1 if x < y else 1
    if a[1] == b[1]:
        return 0
    if not a[1]:
        return 1
    if not b[1]:
        return -1
    return -1 if a[1] < b[1] else 1


def _clean(text):
    """Drop remarks like '(yet to be released)' and commit hashes in parentheses."""
    return re.sub(r"\([^)]*\)", "", text or "").strip()


def _terms(text):
    """'>= 3.0, < 3.3.8' -> [('>=', v), ('<', v)]; None if a term is not a version comparison."""
    out = []
    for item in _clean(text).split(","):
        item = item.strip()
        if not item:
            continue
        m = re.match(r"^(<=|>=|<|>|=)?\s*(\S+)$", item)
        v = parse_version(m.group(2)) if m else None
        if v is None:
            return None
        out.append((m.group(1) or "", v))
    return out or None


def _holds(op, ours, v):
    c = _cmp(ours, v)
    return {"<": c < 0, "<=": c <= 0, ">": c > 0, ">=": c >= 0, "=": c == 0}[op]


def affected(version, vulnerable, patched):
    """True, False, or None if the free-text ranges of a repository advisory cannot be read.

    Comparisons with a lower and an upper bound are a range ('>= 3.0, < 3.3.8'); other lists are
    alternatives ('<=3.3.11, 4.0.0-alpha1'). A bare version ('v5.5.5', the ESP-IDF style) means that
    release series up to this version: affected if major.minor are equal and ours is not newer. A
    patched version of the same series that ours reaches clears the finding."""
    ours = parse_version(version)
    terms = _terms(vulnerable)
    if ours is None or terms is None:
        return None
    ops = [op for op, _ in terms]
    if len(terms) == 2 and any(o.startswith(">") for o in ops) and any(o.startswith("<") for o in ops):
        hit = all(_holds(op, ours, v) for op, v in terms)
    else:
        hit = False
        for op, v in terms:
            if op:
                hit = hit or _holds(op, ours, v)
            else:
                hit = hit or (ours[0][:2] == v[0][:2] and _cmp(ours, v) <= 0)
    if hit and patched:
        for op, v in _terms(patched) or []:
            if op in ("", ">=", "=") and ours[0][:2] == v[0][:2] and _cmp(ours, v) >= 0:
                return False
    return hit


def _tag_version(repo, commit, token):
    """The newest tag the pinned commit contains (git describe over the API), for git pins."""
    tags = _http_json("%s/repos/%s/tags?per_page=100" % (GITHUB_API, repo), token=token)
    tags = [t for t in tags if parse_version(t["name"])]
    tags.sort(key=lambda t: parse_version(t["name"])[0], reverse=True)
    for t in tags:
        cmp_ = _http_json("%s/repos/%s/compare/%s...%s" % (GITHUB_API, repo, t["commit"]["sha"], commit), token=token)
        if cmp_.get("status") in ("identical", "ahead"):
            return t["name"].lstrip("v")
    return None


def collect_pins(token):
    """[(label, repo or None, version, commit)] of the firmware and [(name, version)] of the flasher."""
    pins = []
    for spec in ini_list("esp32", "lib_deps"):
        m = re.match(r"^https://github\.com/([\w.-]+/[\w.-]+?)(?:\.git)?#([0-9a-f]{40})$", spec)
        if m:
            repo, commit = m.group(1), m.group(2)
            pins.append((repo.split("/")[1], repo, _tag_version(repo, commit, token), commit))
            continue
        p = parse_spec(spec)
        if not p or p[0] not in LIB_REPOS:
            raise CheckError("lib_deps: no upstream repository known for %r (add it to LIB_REPOS)" % spec)
        pins.append((p[0], LIB_REPOS[p[0]], p[1], None))
    with open(NOTICES, encoding="utf-8") as f:
        notes = notice_versions(f.read())
    pins.append(("Arduino-ESP32", "espressif/arduino-esp32", notes["arduino"], None))
    idf_tag = notes["esp-idf"].split()[0]
    m = re.search(r"`([0-9a-f]{7,40})`, branch ", notes["esp-idf"])
    if m:  # branch build (E-804): the commit must contain the tag the notices name
        cmp_ = _http_json("%s/repos/espressif/esp-idf/compare/%s...%s" % (GITHUB_API, idf_tag, m.group(1)), token=token)
        if cmp_.get("status") not in ("identical", "ahead"):
            raise CheckError("ESP-IDF commit %s does not contain %s (THIRD_PARTY_NOTICES.md)" % (m.group(1), idf_tag))
    pins.append(("ESP-IDF", "espressif/esp-idf", idf_tag.lstrip("v"), None))
    lock = flasher_site.read_json(LOCK_FILE)
    npm = {}
    for url in [lock["base"]] + [n["url"] for n in lock.get("notices", {}).values()]:
        m = re.match(r"^https://cdn\.jsdelivr\.net/npm/((?:@[\w.-]+/)?[\w.-]+)@([^/]+)/", url)
        if m:
            npm[m.group(1)] = m.group(2)
    for name, repo in NPM_REPOS.items():
        if name in npm:
            pins.append((name, repo, npm[name], None))
    return pins, sorted(npm.items())


def cmd_advisories(args):
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    with open(ACCEPTED_FILE, encoding="utf-8") as f:
        accepted = json.load(f)
    pins, npm = collect_pins(token)
    overrides = dict(o.split("=", 1) for o in args.override or [])
    pins = [(l, r, overrides.get(l, v), c) for l, r, v, c in pins]
    npm = [(n, overrides.get(n, v)) for n, v in npm]
    findings, notes, seen = [], [], set()
    for label, repo, version, commit in pins:
        if version is None:
            findings.append((label, version, "-", "no version for the pinned commit %s" % commit[:7], ""))
            continue
        for adv in _http_json("%s/repos/%s/security-advisories?state=published&per_page=100" % (GITHUB_API, repo), token=token):
            if adv.get("withdrawn_at"):
                continue
            states = [affected(version, v.get("vulnerable_version_range"), v.get("patched_versions"))
                      for v in adv.get("vulnerabilities") or []] or [None]
            if True in states:
                state = "affected"
            elif None in states:
                state = "range not readable"
            else:
                continue
            gid = adv["ghsa_id"]
            seen.add(gid)
            if gid in accepted:
                notes.append("%s %s (%s): accepted, %s" % (label, version, gid, accepted[gid]["reason"]))
                continue
            findings.append((label, version, gid, "%s: %s (%s)" % (state, adv.get("summary", ""), adv.get("severity", "?")),
                             adv.get("html_url", "")))
    # The flasher's npm packages and the git pins against OSV (it includes the GitHub advisories).
    queries = [{"package": {"ecosystem": "npm", "name": n}, "version": v} for n, v in npm]
    labels = ["%s %s" % (n, v) for n, v in npm]
    for label, _, _, commit in pins:
        if commit:
            queries.append({"commit": commit})
            labels.append("%s %s" % (label, commit[:7]))
    res = _http_json(OSV_API + "/querybatch", {"queries": queries}).get("results", [])
    for label, r in zip(labels, res):
        for v in r.get("vulns") or []:
            vid = v["id"]
            seen.add(vid)
            if vid in accepted:
                notes.append("%s (%s): accepted, %s" % (label, vid, accepted[vid]["reason"]))
                continue
            name, _, version = label.rpartition(" ")
            findings.append((name, version, vid, "affected (OSV)", "https://osv.dev/vulnerability/" + vid))
    stale = sorted(set(accepted) - seen)
    lines = ["### Dependency advisories", "",
             "Checked %d pins (GitHub repository advisories) and %d npm packages plus %d git commits (OSV)."
             % (len(pins), len(npm), sum(1 for p in pins if p[3])), ""]
    if findings:
        lines += ["| Dependency | Version | Advisory | Finding |", "|---|---|---|---|"]
        lines += ["| %s | %s | %s | %s |" % (l, v, ("[%s](%s)" % (g, u)) if u else g, t.replace("|", "/"))
                  for l, v, g, t, u in findings]
        lines += ["", "Update the dependency, or record why the advisory does not apply in "
                  "`tools/advisories_accepted.json`."]
    else:
        lines.append("No open finding.")
    if notes:
        lines += ["", "Accepted (`tools/advisories_accepted.json`):", ""] + ["- " + n for n in notes]
    if stale:
        lines += ["", "Accepted entries that no longer match (remove them): " + ", ".join(stale)]
    text = "\n".join(lines) + "\n"
    if args.summary:
        with open(args.summary, "a", encoding="utf-8", newline="\n") as f:
            f.write(text)
    sys.stdout.write(text)
    if findings and args.issue_body:
        with open(args.issue_body, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    if findings:
        raise CheckError("%d advisory finding(s)" % len(findings))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("packages", help="compare the resolved packages with the pins and the notices")
    p.add_argument("--pkg-list", required=True, help="output of pio pkg list -e <env>")
    p.add_argument("--metadata", required=True, help="output of pio project metadata --json-output")
    p.add_argument("--env", required=True)
    p.add_argument("--summary", help="append the Markdown table to this file instead of stdout")
    p.set_defaults(func=cmd_packages)
    p = sub.add_parser("advisories", help="check the pinned versions against security advisories")
    p.add_argument("--summary", help="append the Markdown report to this file (job summary)")
    p.add_argument("--issue-body", help="write the report here when there is a finding")
    p.add_argument("--override", action="append", metavar="NAME=VERSION",
                   help="check this version instead of the pinned one (test), repeatable")
    p.set_defaults(func=cmd_advisories)
    args = ap.parse_args()
    try:
        args.func(args)
    except CheckError as e:
        print("error: %s" % e, file=sys.stderr)
        return 1
    except OSError as e:  # network: urllib errors are OSError
        print("error: %s" % e, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
