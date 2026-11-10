#!/usr/bin/env python3
"""Renders the documentation images in docs/img/ (Python 3 standard library plus the tools below).

    python tools/render_images.py              # everything
    python tools/render_images.py screens      # device screens from the real GUI code
    python tools/render_images.py case         # OpenSCAD renders of the case
    python tools/render_images.py web          # web UI on tools/web_mock.py (127.0.0.1 only)
    python tools/render_images.py social       # 1280 x 640 social preview from the images above

screens: builds the host renderer (`pio run -e screens-win`, `screens` on Linux/macOS; see
tools/screens/), which runs src/gui with LVGL 8.4 on the fakes of test/fakes and writes raw
RGB565 frames; they are scaled 2x (nearest neighbour) into a thin bezel. Sample state only:
invented networks ("Lab", "Workshop"), passwords and a locally administered MAC.
case: OpenSCAD views of the modules of case/CYD_Si5351_Gehaeuse.scad with the MAIN screen on the
display, rendered at twice the size and scaled down by headless Chrome/Edge for smooth edges.
web: tools/web_mock.py on a free port of 127.0.0.1 with the state of the device screens, shot by
headless Chrome or Edge over the DevTools protocol (tools/cdp_shot.mjs, Node 22+).
social: the case on a dark background with a device screen and the phone web UI, 1280 x 640 for
GitHub's social preview (uploaded by hand in the repository settings).

Tools: pio (PIO env or ~/.platformio/penv), openscad (OPENSCAD env or PATH or the default
install path), chrome/msedge (CHROME env, PATH or the default install paths), node (NODE env or
PATH).
"""
import argparse
import http.client
import json
import os
import platform
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import zlib

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
IMG = os.path.join(ROOT, "docs", "img")
SCREENS = ["main", "ch1", "numpad", "ch3", "config", "config-info", "wifi", "wifi-scan", "wifi-device",
           "wifi-ap", "dialog-network-change"]
TOKEN = "render-images-sample-token"


# ---------------------------------------------------------------------------------------------
# PNG

def same_png(path, ihdr, raw):
    """True if the PNG at path has this header and these filtered rows (other chunks ignored)."""
    try:
        data = open(path, "rb").read()
    except OSError:
        return False
    pos, head, idat = 8, b"", b""
    while pos + 8 <= len(data):
        n, tag = struct.unpack(">I4s", data[pos:pos + 8])
        if tag == b"IHDR":
            head = data[pos + 8:pos + 8 + n]
        elif tag == b"IDAT":
            idat += data[pos + 8:pos + 8 + n]
        pos += 12 + n
    try:
        return head == ihdr and zlib.decompress(idat) == raw
    except zlib.error:
        return False


def write_png(path, width, height, rows, alpha):
    """rows: list of bytes, RGB or RGBA per pixel. Per row the filter (None/Sub/Up) with the
    smallest sum is chosen, which keeps the flat GUI colors small."""
    bpp = 4 if alpha else 3
    raw = bytearray()
    prev = bytes(width * bpp)
    for row in rows:
        row = bytes(row)
        sub = bytes((row[i] - (row[i - bpp] if i >= bpp else 0)) & 0xFF for i in range(len(row)))
        up = bytes((row[i] - prev[i]) & 0xFF for i in range(len(row)))
        best = min(((0, row), (1, sub), (2, up)), key=lambda c: sum(b if b < 128 else 256 - b for b in c[1]))
        raw.append(best[0])
        raw += best[1]
        prev = row

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6 if alpha else 2, 0, 0, 0)
    if same_png(path, ihdr, bytes(raw)):
        return  # same picture: keep the file, so a new render does not churn the repository
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
                + chunk(b"IEND", b""))


def rgb565_to_rgb(data):
    out = []
    for i in range(0, len(data), 2):
        v = data[i] | (data[i + 1] << 8)
        r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
        out.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    return out


def screen_png(raw_path, png_path, scale=2, pad=14, radius=20):
    """320 x 240 RGB565 -> scale x nearest neighbour inside a dark rounded bezel (RGBA)."""
    w, h = 320, 240
    with open(raw_path, "rb") as f:
        px = rgb565_to_rgb(f.read())
    sw, sh = w * scale, h * scale
    tw, th = sw + 2 * pad, sh + 2 * pad
    bezel, edge = (24, 27, 32), (58, 64, 72)

    def coverage(x, y):
        # Rounded outer corner, 4x4 supersampled.
        cx = radius if x < radius else (tw - radius - 1 if x > tw - radius - 1 else x)
        cy = radius if y < radius else (th - radius - 1 if y > th - radius - 1 else y)
        if cx == x or cy == y:
            return 1.0
        n = 0
        for sy in range(4):
            for sx in range(4):
                dx, dy = x + (sx + 0.5) / 4 - (cx + 0.5), y + (sy + 0.5) / 4 - (cy + 0.5)
                n += dx * dx + dy * dy <= radius * radius
        return n / 16

    rows = []
    for y in range(th):
        row = bytearray()
        for x in range(tw):
            if pad <= x < pad + sw and pad <= y < pad + sh:
                r, g, b = px[((y - pad) // scale) * w + (x - pad) // scale]
                row += bytes((r, g, b, 255))
                continue
            a = coverage(x, y)
            if a <= 0:
                row += b"\0\0\0\0"
                continue
            # 1 px lighter outer edge and 1 px edge around the glass.
            outer = a < 1 or x == 0 or y == 0 or x == tw - 1 or y == th - 1
            inner = (pad - 1 <= x <= pad + sw and pad - 1 <= y <= pad + sh)
            c = edge if outer or inner else bezel
            row += bytes((c[0], c[1], c[2], int(round(255 * a))))
        rows.append(row)
    write_png(png_path, tw, th, rows, True)


# ---------------------------------------------------------------------------------------------
# Tools

def find_pio():
    if os.environ.get("PIO"):
        return os.environ["PIO"]
    for p in (shutil.which("pio"), shutil.which("platformio")):
        if p:
            return p
    home = os.path.expanduser("~")
    for p in (os.path.join(home, ".platformio", "penv", "Scripts", "pio.exe"),
              os.path.join(home, ".platformio", "penv", "bin", "pio")):
        if os.path.exists(p):
            return p
    sys.exit("pio not found (set PIO)")


def find_openscad():
    if os.environ.get("OPENSCAD"):
        return os.environ["OPENSCAD"]
    p = shutil.which("openscad")
    if p:
        return p
    for p in (r"C:\Program Files\OpenSCAD\openscad.exe", r"C:\Program Files\OpenSCAD (Nightly)\openscad.exe",
              "/Applications/OpenSCAD.app/Contents/MacOS/OpenSCAD"):
        if os.path.exists(p):
            return p
    sys.exit("openscad not found (set OPENSCAD)")


def find_chrome():
    if os.environ.get("CHROME"):
        return os.environ["CHROME"]
    for name in ("chrome", "google-chrome", "chromium", "chromium-browser", "msedge"):
        p = shutil.which(name)
        if p:
            return p
    for p in (r"C:\Program Files\Google\Chrome\Application\chrome.exe",
              r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
              "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"):
        if os.path.exists(p):
            return p
    sys.exit("Chrome or Edge not found (set CHROME)")


def chrome_shot(url, out, width, height, scale=1, budget_ms=4000):
    """Headless screenshot of url (viewport width x height CSS px, device scale factor scale)."""
    profile = tempfile.mkdtemp(prefix="render-images-")
    try:
        cmd = [find_chrome(), "--headless=new", "--disable-gpu", "--hide-scrollbars", "--no-first-run",
               "--no-default-browser-check", "--disable-extensions", "--user-data-dir=" + profile,
               "--window-size=%d,%d" % (width, height), "--force-device-scale-factor=%s" % scale,
               "--virtual-time-budget=%d" % budget_ms, "--screenshot=" + os.path.abspath(out), url]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=120)
    finally:
        shutil.rmtree(profile, ignore_errors=True)
    if not os.path.exists(out):
        sys.exit("no screenshot from " + url)


def file_url(path):
    p = os.path.abspath(path).replace("\\", "/")
    return "file:///" + p.lstrip("/")


def git(*args):
    try:
        return subprocess.run(["git"] + list(args), cwd=ROOT, check=True, capture_output=True,
                              text=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def size_kb(path):
    return os.path.getsize(path) / 1024


# ---------------------------------------------------------------------------------------------
# Device screens

def build_renderer(skip_build):
    env = "screens-win" if platform.system() == "Windows" else "screens"
    if not skip_build:
        subprocess.run([find_pio(), "run", "-e", env], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    return os.path.join(ROOT, ".pio", "build", env, "program.exe" if env == "screens-win" else "program")


def render_screens(skip_build):
    exe = build_renderer(skip_build)
    tmp = tempfile.mkdtemp(prefix="screens-")
    try:
        # Build line of the CONFIG info card: fixed sample values, so the picture only changes
        # when the GUI does (a real commit would change it with every commit).
        commit = "1a2b3c4"
        when = "2026-11-10 12:00"
        subprocess.run([exe, "--out", tmp, "--branch", "main", "--commit", commit, "--time", when],
                       check=True, stdout=subprocess.DEVNULL)
        for name in SCREENS:
            out = os.path.join(IMG, "screen-%s.png" % name)
            screen_png(os.path.join(tmp, name + ".rgb565"), out)
            print("%-40s %6.1f KB" % (os.path.relpath(out, ROOT), size_kb(out)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ---------------------------------------------------------------------------------------------
# Case

CASE_SCAD = os.path.join(ROOT, "case", "CYD_Si5351_Gehaeuse.scad")
CASE_COLOR = "#56687a"
# name: (view, camera rot x,y,z, width, height, zoom of the --viewall frame)
CASE_VIEWS = {
    "case-assembly": ("assembly", "58,0,28", 1200, 760, 1.15),
    "case-explode": ("explode", "58,0,30", 1000, 1000, 1.0),
    "case-print": ("print", "48,0,22", 1200, 760, 1.1),
    "case-sma-side": ("assembly", "80,0,195", 1200, 640, 1.3),
    "case-usb-side": ("assembly", "74,0,118", 1200, 700, 1.2),
}


def case_param(text, name):
    m = re.search(r"\b%s\s*=\s*([-\d.]+)\s*;" % name, text)
    if not m:
        sys.exit("%s not found in %s" % (name, CASE_SCAD))
    return float(m.group(1))


def screen_scad(raw_path, case_text):
    """The MAIN screen as thin colored boxes on the display of the CYD dummy (equal runs of
    a row merged with the same run of the rows below). Pixel (0, 0) is at the top left with
    the USB-C side on the right, the image top towards the SMA side."""
    x0, x1 = case_param(case_text, "act_x0"), case_param(case_text, "act_x1")
    y0, y1 = case_param(case_text, "act_y0"), case_param(case_text, "act_y1")
    z = case_param(case_text, "disp_top") + 0.06
    with open(raw_path, "rb") as f:
        px = rgb565_to_rgb(f.read())
    sx, sy = (x1 - x0) / 320, (y1 - y0) / 240
    open_rects, done = {}, []
    for y in range(240):
        runs, start = [], 0
        for x in range(1, 321):
            if x == 320 or px[y * 320 + x] != px[y * 320 + start]:
                runs.append((start, x, px[y * 320 + start]))
                start = x
        now = {}
        for r in runs:
            if r in open_rects:
                now[r] = open_rects.pop(r)
            else:
                now[r] = y
        done += [(r, top, y) for r, top in open_rects.items()]
        open_rects = now
    done += [(r, top, 240) for r, top in open_rects.items()]
    by_color = {}
    for (a, b, c), top, bottom in done:
        by_color.setdefault(c, []).append("translate([%.3f,%.3f,%.2f])cube([%.3f,%.3f,0.02]);"
                                          % (x0 + a * sx, y1 - bottom * sy, z, (b - a) * sx, (bottom - top) * sy))
    lines = ["module screen() {"]
    for c, cubes in by_color.items():
        lines.append("color([%.3f,%.3f,%.3f]) {%s}" % (c[0] / 255, c[1] / 255, c[2] / 255, "".join(cubes)))
    lines.append("}")
    return "\n".join(lines)


def case_scad(view, screen):
    """A view of the case file's modules (default parameters: snap hooks, rail), the printed
    parts in one color, the dummies in theirs."""
    use = CASE_SCAD.replace("\\", "/")
    body = {
        # Wider apart than part = "explode": top, CYD and bottom with the Si5351 module.
        "explode": "translate([0,0,64]) color(c) top(); translate([0,0,34]) { cyd_dummy(); screen(); }"
                   " color(c) bottom(); si_dummy();",
        # As part = "print": top upside down next to the bottom. Offsets: outer height above the
        # parting plane (8.3), depth below it (24), outer width (57) + 8 mm, default parameters.
        "print": "color(c) { translate([0,0,8.3]) rotate([180,0,0]) top(); translate([0,65,24]) bottom(); }",
        "assembly": "color(c) top(); cyd_dummy(); screen(); color(c) bottom(); si_dummy();",
    }[view]
    return 'use <%s>\nc = "%s";\n%s\n%s\n' % (use, CASE_COLOR, screen, body)


def downscale(src, out, width, height, zoom=1.0):
    """Scales a 2x render to width x height with Chrome's smooth filter; zoom > 1 crops the
    empty margin that --viewall leaves around the object."""
    html = src + ".html"
    w, h = width * zoom, height * zoom
    with open(html, "w", encoding="utf-8") as f:
        f.write("<!doctype html><html><head><style>html,body{margin:0;overflow:hidden;background:#f8f8f8}"
                "img{position:absolute;left:%.1fpx;top:%.1fpx;width:%.1fpx;height:%.1fpx}</style></head>"
                "<body><img src=\"%s\"></body></html>"
                % ((width - w) / 2, (height - h) / 2, w, h, os.path.basename(src)))
    chrome_shot(file_url(html), out, width, height, budget_ms=1000)


def case_view(tmp, screen, name, view, rot, w, h, zoom, out, scheme="Tomorrow"):
    scad = os.path.join(tmp, name + ".scad")
    with open(scad, "w", encoding="utf-8") as f:
        f.write(case_scad(view, screen))
    big = os.path.join(tmp, name + ".png")
    subprocess.run([find_openscad(), "-o", big, "--render", "--backend=Manifold",
                    "--imgsize=%d,%d" % (2 * w, 2 * h), "--camera=0,0,0,%s,0" % rot, "--viewall", "--autocenter",
                    "--projection=p", "--colorscheme=" + scheme, scad],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    downscale(big, out, w, h, zoom)


def case_screen(tmp, skip_build):
    """The MAIN screen for the display of the case renders (the device screens' renderer)."""
    with open(CASE_SCAD, encoding="utf-8") as f:
        case_text = f.read()
    subprocess.run([build_renderer(skip_build), "--out", tmp], check=True, stdout=subprocess.DEVNULL)
    return screen_scad(os.path.join(tmp, "main.rgb565"), case_text)


def render_case(skip_build):
    tmp = tempfile.mkdtemp(prefix="case-")
    try:
        screen = case_screen(tmp, skip_build)
        for name, (view, rot, w, h, zoom) in CASE_VIEWS.items():
            out = os.path.join(IMG, name + ".png")
            case_view(tmp, screen, name, view, rot, w, h, zoom, out)
            print("%-40s %6.1f KB" % (os.path.relpath(out, ROOT), size_kb(out)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


# ---------------------------------------------------------------------------------------------
# Web UI

def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def post(port, path, body):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
    c.request("POST", path, json.dumps(body),
              {"Content-Type": "application/json", "X-CYD-Token": TOKEN, "Host": "127.0.0.1:%d" % port})
    r = c.getresponse()
    r.read()
    c.close()
    if r.status >= 300:
        sys.exit("mock answered %d to %s" % (r.status, path))


def render_web():
    port = free_port()
    mock = subprocess.Popen([sys.executable, os.path.join(ROOT, "tools", "web_mock.py"), "--host", "127.0.0.1",
                             "--port", str(port), "--token", TOKEN],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(50):
            try:
                socket.create_connection(("127.0.0.1", port), timeout=0.2).close()
                break
            except OSError:
                time.sleep(0.1)
        # The state of the device screens (spaced writes: 20/s at most).
        for path, body in (("/api/channel/1", {"frequency": 7100000}), ("/api/channel/2", {"frequency": 14000000}),
                           ("/api/ch3", {"master": 1, "phase": 90}), ("/api/channel/1", {"enabled": True}),
                           ("/api/channel/2", {"enabled": True}), ("/api/ch3", {"enabled": True})):
            post(port, path, body)
            time.sleep(0.1)
        # REMOTE WEB goes after 10 s, the autosave after 5 s.
        time.sleep(10.5)
        base = "http://127.0.0.1:%d/" % port
        live = "document.getElementById('conn').textContent === 'Live'"
        shots = [
            # Header, MAIN and the channel areas.
            {"name": "web-desktop", "url": base, "width": 1280, "height": 625},
            # The WiFi area (sample networks of the mock).
            {"name": "web-wifi", "url": base, "width": 1280, "height": 900, "clip": "#wifi", "clipPad": 6},
            # Phone: header at the top, output bar at the bottom.
            {"name": "web-phone", "url": base, "width": 390, "height": 844, "scale": 2, "mobile": True},
        ]
        for shot in shots:
            shot.update(out=os.path.join(IMG, shot.pop("name") + ".png"), waitFor=live)
        spec = os.path.join(tempfile.mkdtemp(prefix="web-"), "spec.json")
        with open(spec, "w", encoding="utf-8") as f:
            json.dump(shots, f)
        node = os.environ.get("NODE") or shutil.which("node") or sys.exit("node not found (set NODE)")
        subprocess.run([node, os.path.join(ROOT, "tools", "cdp_shot.mjs"), find_chrome(), spec], check=True,
                       stdout=subprocess.DEVNULL)
        shutil.rmtree(os.path.dirname(spec), ignore_errors=True)
        for shot in shots:
            print("%-40s %6.1f KB" % (os.path.relpath(shot["out"], ROOT), size_kb(shot["out"])))
    finally:
        mock.terminate()
        mock.wait(timeout=10)


# ---------------------------------------------------------------------------------------------
# Social preview

SOCIAL_HTML = """<!doctype html><html><head><meta charset="utf-8"><style>
html,body{margin:0;width:1280px;height:640px;overflow:hidden}
body{background:radial-gradient(circle at 72% 40%,#1d2530 0,#0b0f14 70%);color:#eceff1;
 font-family:"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;position:relative}
.txt{position:absolute;left:64px;top:70px;width:520px}
h1{font-size:66px;line-height:1.02;margin:0 0 22px;font-weight:700;letter-spacing:-1px}
h1 span{color:#fdd835}
.tag{font-size:27px;line-height:1.3;color:#cfd8dc;margin:0 0 34px}
ul{list-style:none;margin:0;padding:0;font-size:21px;line-height:1.75;color:#b0bec5}
li::before{content:"";display:inline-block;width:12px;height:12px;border-radius:3px;margin-right:14px;
 background:var(--c);vertical-align:1px}
.url{position:absolute;left:64px;bottom:44px;font-size:19px;color:#78909c}
.case{position:absolute;left:520px;top:-10px;width:800px;height:500px;
 -webkit-mask-image:radial-gradient(ellipse 50% 50% at 50% 50%,#000 62%,transparent 100%)}
.screen{position:absolute;left:584px;top:350px;width:338px;filter:drop-shadow(0 10px 22px rgba(0,0,0,.6))}
.phone{position:absolute;left:1004px;top:226px;width:212px;height:372px;object-fit:cover;object-position:top;
 border-radius:22px;border:6px solid #2a3340;box-shadow:0 10px 30px rgba(0,0,0,.6)}
</style></head><body>
<div class="txt"><h1>CYD HF <span>Generator</span></h1>
<p class="tag">3-channel SI5351 HF generator on the ESP32 Cheap&nbsp;Yellow&nbsp;Display</p>
<ul><li style="--c:#e53935">CH1, CH2: 8 kHz &ndash; 160 MHz</li>
<li style="--c:#1e88e5">CH3: IQ 0/90/180/270&deg; to CH1 or CH2</li>
<li style="--c:#43a047">Touch &middot; web UI &middot; SCPI over USB and TCP</li>
<li style="--c:#fdd835">Open source firmware and 3D-printed case</li></ul></div>
<div class="url">github.com/dm8sb/CYD-HF-Generator</div>
<img class="case" src="{case}"><img class="screen" src="{screen}"><img class="phone" src="{phone}">
</body></html>
"""


def render_social(skip_build):
    """1280 x 640 for GitHub (Settings > Social preview, uploaded by hand): the case on a dark
    background (Tomorrow Night), the CH1 screen and the phone web UI from docs/img."""
    for need in ("screen-ch1.png", "web-phone.png"):
        if not os.path.exists(os.path.join(IMG, need)):
            sys.exit("render screens and web first (docs/img/%s missing)" % need)
    tmp = tempfile.mkdtemp(prefix="social-")
    try:
        screen = case_screen(tmp, skip_build)
        case = os.path.join(tmp, "case-dark.png")
        case_view(tmp, screen, "case-dark", "assembly", "58,0,28", 800, 500, 1.2, case, "Tomorrow Night")
        html = os.path.join(tmp, "social.html")
        with open(html, "w", encoding="utf-8") as f:
            f.write(SOCIAL_HTML.replace("{case}", file_url(case))
                    .replace("{screen}", file_url(os.path.join(IMG, "screen-ch1.png")))
                    .replace("{phone}", file_url(os.path.join(IMG, "web-phone.png"))))
        out = os.path.join(IMG, "social-preview.png")
        chrome_shot(file_url(html), out, 1280, 640, budget_ms=1500)
        print("%-40s %6.1f KB" % (os.path.relpath(out, ROOT), size_kb(out)))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("what", nargs="*", default=["all"], choices=["all", "screens", "case", "web", "social"])
    ap.add_argument("--skip-build", action="store_true", help="screens: use the renderer built before")
    args = ap.parse_args()
    what = {"screens", "case", "web", "social"} if "all" in args.what else set(args.what)
    os.makedirs(IMG, exist_ok=True)
    if "screens" in what:
        render_screens(args.skip_build)
    if "case" in what:
        render_case(args.skip_build or "screens" in what)
    if "web" in what:
        render_web()
    if "social" in what:
        render_social(args.skip_build or "screens" in what or "case" in what)


if __name__ == "__main__":
    main()
