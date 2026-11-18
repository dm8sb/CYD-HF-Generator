#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Contract check of the web server (S55, S56): REST, guard answers, WebSocket push, scan, login, logout.

Python 3 standard library only. Runs against the device or tools/web_mock.py and prints one
PASS/FAIL line per check, like device_check.py does for SCPI. It changes the step, the CH2
frequency and the CH2 coupling (E-52) briefly and restores them; it never switches an output on
and never changes WiFi.

    python tools/web_check.py cyd-hf-1a2b.local            # the device (port 80)
    python tools/web_check.py 127.0.0.1:8080               # the mock
    python tools/web_check.py 192.168.1.50 --password      # asks for the web password
    python tools/web_check.py 192.168.4.1 --skip-scan      # over the AP
"""
import argparse
import base64
import getpass
import json
import os
import re
import socket
import struct
import sys
import time
import urllib.error
import urllib.request

import device_constants as dc
import hostport

TOKEN_RE = re.compile(r'<meta\s+name="cyd-token"\s+content="([0-9a-f]{32})"', re.I)
STATE_KEYS = {"rev", "hw", "save", "remote", "ch", "ch3", "limits", "config", "wifi", "device"}
# state.limits must match include/constants.h (AF48): the web UI takes range and steps from it.
LIMITS = {"fmin": dc.FREQ_MIN, "fmax": dc.FREQ_MAX, "steps": list(dc.STEPS), "offsetMax": dc.OFFSET_MAX}

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print("%s  %s%s" % ("PASS" if ok else "FAIL", name, ("  (" + detail + ")") if detail and not ok else ""))
    return ok


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k):
        return None


class Api:
    def __init__(self, target):
        self.addr = hostport.split(target, 80)  # ValueError for a bad host:port
        self.hostport = target.strip()
        self.base = "http://" + self.hostport
        self.token = ""
        self.cookie = ""
        self.opener = urllib.request.build_opener(NoRedirect)

    def raw(self, method, path, body=None, headers=None, ctype="application/json", token=True):
        h = {}
        if self.cookie:
            h["Cookie"] = self.cookie
        if body is not None:
            h["Content-Type"] = ctype
        if method == "POST" and token:
            h["X-CYD-Token"] = self.token
        h.update(headers or {})
        data = body if isinstance(body, bytes) or body is None else json.dumps(body).encode()
        req = urllib.request.Request(self.base + path, data=data, headers=h, method=method)
        try:
            with self.opener.open(req, timeout=8) as r:
                return r.status, dict(r.headers), r.read()
        except urllib.error.HTTPError as e:
            return e.code, dict(e.headers), e.read()

    def call(self, method, path, body=None, **kw):
        st, hd, raw = self.raw(method, path, body, **kw)
        try:
            j = json.loads(raw.decode("utf-8")) if raw else None
        except ValueError:
            j = raw.decode("utf-8", "replace")
        return st, hd, j


def err(j):
    return j.get("error", "") if isinstance(j, dict) else ""


# --- minimal RFC 6455 client --------------------------------------------------------------
def ws_open(target, path, timeout=5, cookie=""):
    s = socket.create_connection(hostport.split(target, 80), timeout=timeout)
    key = base64.b64encode(os.urandom(16)).decode()
    extra = "Cookie: %s\r\n" % cookie if cookie else ""
    s.sendall(("GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n%s\r\n" % (path, target, key, extra)).encode())
    head = b""
    while b"\r\n\r\n" not in head:
        part = s.recv(1024)
        if not part:
            break
        head += part
    status = int(head.split(b" ", 2)[1]) if head.startswith(b"HTTP/1.1 ") else 0
    rest = head.split(b"\r\n\r\n", 1)[1] if b"\r\n\r\n" in head else b""
    return s, status, rest


def ws_frame(op, data=b""):
    mask = os.urandom(4)
    n = len(data)
    hdr = bytes([0x80 | op]) + (bytes([0x80 | n]) if n < 126 else bytes([0x80 | 126]) + struct.pack(">H", n))
    return hdr + mask + bytes(c ^ mask[i % 4] for i, c in enumerate(data))


def ws_read(s, buf, timeout=5):
    """Next frame (opcode, payload); answers pings. Returns (op, data, rest) or (None, b"", buf)."""
    s.settimeout(timeout)
    end = time.time() + timeout
    while time.time() < end:
        if len(buf) >= 2:
            op, ln = buf[0] & 0x0F, buf[1] & 0x7F
            off = 2
            if ln == 126 and len(buf) >= 4:
                ln, off = struct.unpack(">H", buf[2:4])[0], 4
            elif ln == 127 and len(buf) >= 10:
                ln, off = struct.unpack(">Q", buf[2:10])[0], 10
            if ln < 126 or off > 2:
                if len(buf) >= off + ln:
                    data, buf = buf[off:off + ln], buf[off + ln:]
                    if op == 0x9:
                        s.sendall(ws_frame(0xA, data))
                        continue
                    return op, data, buf
        try:
            part = s.recv(4096)
        except socket.timeout:
            break
        if not part:
            return 0x8, b"", buf
        buf += part
    return None, b"", buf


def check_page(api):
    """Page and assets (W-S7); sets api.token. False if the page has no token."""
    st, hd, raw = api.raw("GET", "/")
    page = raw.decode("utf-8", "replace")
    m = TOKEN_RE.search(page)
    check("GET / 200 with per-boot token", st == 200 and m is not None, "HTTP %d" % st)
    if not m:
        return False
    api.token = m.group(1)
    # Footer (E-39): source and notices at a release tag, no placeholder left.
    check("footer links the release tag", "{{" not in page and "/blob/main/" not in page
          and re.search(r'CYD-HF-Generator/tree/v[0-9A-Za-z.+-]+"', page) is not None)
    csp = hd.get("Content-Security-Policy", "")
    check("CSP script-src 'self', frame-ancestors 'none'", "script-src 'self'" in csp and "frame-ancestors 'none'" in csp, csp)
    check("nosniff, no-referrer", hd.get("X-Content-Type-Options") == "nosniff" and hd.get("Referrer-Policy") == "no-referrer")
    check("page not stored", hd.get("Cache-Control") in ("no-store", "no-cache"), hd.get("Cache-Control", ""))
    # Every <script> must load a file (src=), no handler attributes or inline styles; case-insensitive
    # (CodeQL py/bad-tag-filter).
    scripts = re.findall(r"<script\b[^>]*>", page, re.I)
    check("no inline script or handler", all(re.search(r"\ssrc=", s, re.I) for s in scripts)
          and not re.search(r"\son\w+\s*=|\sstyle\s*=", page, re.I))
    st, hd, raw = api.raw("GET", "/app.js", headers={"Accept-Encoding": "gzip"})
    check("GET /app.js 200", st == 200 and len(raw) > 1000, "HTTP %d" % st)
    if hd.get("ETag"):
        check("app.js gzip", hd.get("Content-Encoding") == "gzip", hd.get("Content-Encoding", ""))
        st2, _, _ = api.raw("GET", "/app.js", headers={"If-None-Match": hd["ETag"]})
        check("app.js If-None-Match -> 304", st2 == 304, "HTTP %d" % st2)
    st, _, _ = api.raw("GET", "/style.css")
    check("GET /style.css 200", st == 200)
    st, _, _ = api.raw("GET", "/README.md")
    check("README.md not served", st == 404, "HTTP %d" % st)
    return True


def check_login(api, password):
    """With a password only the page and the LXI identification are readable until the login
    (D25, D33); then logs in and keeps the session cookie in api."""
    st, _, j = api.call("GET", "/api/state")
    check("state without login -> 401 (D25)", st == 401 and "Login required" in err(j), "%d %s" % (st, j))
    st, _, _ = api.call("GET", "/api/token")
    check("token without login -> 401 (D25)", st == 401, "HTTP %d" % st)
    s, st, _ = ws_open(api.hostport, "/ws?t=" + api.token)
    check("WebSocket without login -> 401 (D25)", st == 401, "HTTP %d" % st)
    s.close()
    st, _, raw = api.raw("GET", "/lxi/identification")
    check("LXI identification without login -> 200 (D33)", st == 200 and b"<LXIDevice" in raw, "HTTP %d" % st)
    st, _, j = api.call("POST", "/api/config", {"step": 1000})
    check("write without login -> 401", st == 401 and "Login required" in err(j), "%d %s" % (st, j))
    st, _, j = api.call("POST", "/api/login", {"password": password + "x"})
    check("wrong password -> 401", st == 401 and "Wrong password" in err(j), "%d %s" % (st, j))
    t0 = time.time()
    st, hd, _ = api.raw("POST", "/api/login", {"password": password})
    cookie = hd.get("Set-Cookie", "")
    check("login -> 204 with HttpOnly SameSite=Strict cookie",
          st == 204 and "HttpOnly" in cookie and "SameSite=Strict" in cookie, "%d %s" % (st, cookie))
    print("      login took %.2f s" % (time.time() - t0))
    api.cookie = cookie.split(";", 1)[0]


def check_state(api):
    """The state contract; returns the state or None."""
    st, hd, s0 = api.call("GET", "/api/state")
    ok = st == 200 and isinstance(s0, dict) and set(s0) == STATE_KEYS
    check("GET /api/state contract keys", ok, "%d %s" % (st, sorted(s0) if isinstance(s0, dict) else s0))
    if not ok:
        return None
    check("/api/* no-store", hd.get("Cache-Control") == "no-store")
    check("wifi.confirmWithin and device.passwordSet",
          isinstance(s0["wifi"].get("confirmWithin"), int) and isinstance(s0["device"].get("passwordSet"), bool))
    check("state.limits as in include/constants.h", s0["limits"] == LIMITS, str(s0["limits"]))
    c2 = s0["ch"][1]
    check("ch[1].coupled and ch[1].offset (E-52)", isinstance(c2.get("coupled"), bool) and type(c2.get("offset")) is int
          and "coupled" not in s0["ch"][0], str({k: c2.get(k) for k in ("coupled", "offset")}))
    check("state has no MAC/commit", "mac" not in json.dumps(s0).lower() and "commit" not in json.dumps(s0).lower())
    st, _, j = api.call("GET", "/api/token")
    check("GET /api/token", st == 200 and isinstance(j, dict) and j.get("token") == api.token, "%d %s" % (st, j))
    return s0


# Guard answers (W-S4, W-S5, W-S10): (name, method, path, body, call options, accepted statuses).
GUARD_CASES = [
    ("POST text/plain -> 415", "POST", "/api/config", b"step=1", {"ctype": "text/plain"}, (415,)),
    ("foreign Origin -> 403", "POST", "/api/config", {"step": 1000}, {"headers": {"Origin": "http://evil.example"}}, (403,)),
    ("Sec-Fetch-Site cross-site -> 403", "POST", "/api/config", {"step": 1000},
     {"headers": {"Sec-Fetch-Site": "cross-site"}}, (403,)),
    ("OPTIONS -> 405", "OPTIONS", "/api/state", None, {}, (405,)),
    ("foreign Host -> 421 (or captive 302 over the AP)", "GET", "/api/state", None, {"headers": {"Host": "evil.example"}},
     (421, 302)),
    ("body > 1 KB -> 413", "POST", "/api/config", b"{" + b" " * 1100 + b"}", {}, (413,)),
    ("URL > 256 B -> 414", "GET", "/api/state?" + "a" * 300, None, {}, (414,)),
]

# REST errors (ARBEITSPLAN 3.6 point 5, 9, 11): (path, body, SCPI code of the 400 answer).
ERROR_CASES = [
    ("/api/config", b"{\"drive\":", "-102"), ("/api/config", {"drive": "4"}, "-104"), ("/api/config", {"drive": 4.5}, "-104"),
    ("/api/config", {"x": 1}, "-108"), ("/api/config", {"drive": 5}, "-224"), ("/api/config", {"correction": 99999}, "-222"),
    ("/api/channel/1", {"frequency": 1}, "-222"), ("/api/channel/1", {"frequency": 7000000, "frequencyText": "7M"}, "-108"),
    ("/api/channel/1", {"frequencyText": "abc"}, "-224"), ("/api/ch3", {"phase": 45}, "-224"), ("/api/ch3", {"master": 3}, "-224"),
    ("/api/config", {}, "-109"),
    ("/api/channel/1", {"coupled": True}, "-108"), ("/api/channel/1", {"offset": 0}, "-108"),
    ("/api/channel/2", {"coupled": 1}, "-104"), ("/api/channel/2", {"offset": "455k"}, "-104"),
    ("/api/channel/2", {"offset": 160000001}, "-222"),
]


def check_guard(api):
    st, _, j = api.call("POST", "/api/config", {"step": 1000}, token=False)
    check("POST without token -> 403 reload text", st == 403 and err(j) == "-200,Invalid token: reload the page", "%d %s" % (st, j))
    for name, method, path, body, kw, want in GUARD_CASES:
        st, _, _ = api.call(method, path, body, **kw)
        check(name, st in want, "HTTP %d" % st)


def check_rest_errors(api, s0):
    for path, body, code in ERROR_CASES:
        st, _, j = api.call("POST", path, body)
        check("POST %s %s -> 400 %s" % (path, body if isinstance(body, dict) else body.decode(), code),
              st == 400 and err(j).startswith(code + ","), "%d %s" % (st, j))
    # DOC-2: backlight is rounded to 10 % instead of refused (restored right after). The error
    # cases above count as writes too: wait, so the 20 writes/s limit does not answer 429.
    time.sleep(1.1)
    bl0 = s0["config"]["backlight"]
    st, _, j = api.call("POST", "/api/config", {"backlight": 55})
    check("POST backlight 55 -> 200, rounded to 60", st == 200 and isinstance(j, dict) and j["config"]["backlight"] == 60,
          "%d %s" % (st, j.get("config", {}).get("backlight") if isinstance(j, dict) else j))
    api.call("POST", "/api/config", {"backlight": bl0})
    st, _, j = api.call("POST", "/api/config", b"[1]")
    check("POST /api/config [1] -> 400 -104", st == 400 and err(j).startswith("-104,"), "%d %s" % (st, j))
    st, _, j = api.call("POST", "/api/config", {"remoteOutputs": "allowed"})
    check("remoteOutputs from the web -> 403", st == 403, "%d %s" % (st, j))
    st, _, j = api.call("POST", "/api/channel/3", {"enabled": False})
    check("/api/channel/3 -> 404", st == 404, "HTTP %d" % st)


def check_writes(api, s0):
    """Writes answer with the state that holds them; the rate limit. Restores step and CH2."""
    step0, f0 = s0["config"]["step"], s0["ch"][1]["frequency"]
    st, _, j = api.call("POST", "/api/config", {"step": 100 if step0 != 100 else 10})
    check("POST step -> 200 state with the new step", st == 200 and isinstance(j, dict) and j["config"]["step"] != step0, "%d" % st)
    f1 = f0 + 1000 if f0 < 150000000 else f0 - 1000
    st, _, j = api.call("POST", "/api/channel/2", {"frequency": f1})
    check("POST CH2 frequency -> 200 state with it", st == 200 and isinstance(j, dict) and j["ch"][1]["frequency"] == f1, "%d" % st)
    if isinstance(j, dict):
        check("REMOTE web after a write", j.get("remote") == "web", str(j.get("remote")))
    api.call("POST", "/api/channel/2", {"frequency": f0})
    api.call("POST", "/api/config", {"step": step0})
    # CH2 coupled to CH1 (E-52): CH1 + offset, the CH2 frequency refused, then restored.
    c2 = s0["ch"][1]
    st, _, j = api.call("POST", "/api/channel/2", {"coupled": True, "offset": -455000})
    want = max(0, s0["ch"][0]["frequency"] - 455000)
    check("POST CH2 coupled, offset -455000 -> state CH1 + offset",
          st == 200 and isinstance(j, dict) and j["ch"][1]["coupled"] is True and j["ch"][1]["offset"] == -455000
          and j["ch"][1]["frequency"] == want, "%d %s" % (st, j["ch"][1] if isinstance(j, dict) and "ch" in j else j))
    st, _, j = api.call("POST", "/api/channel/2", {"frequency": f0})
    check("POST CH2 frequency while coupled -> 400 -221", st == 400 and err(j) == "-221,CH2 is coupled to CH1", "%d %s" % (st, j))
    st, _, j = api.call("POST", "/api/channel/2", {"coupled": c2.get("coupled", False), "offset": c2.get("offset", 0)})
    if not c2.get("coupled", False):
        api.call("POST", "/api/channel/2", {"frequency": f0})
    time.sleep(1.1)
    # Over WiFi one write takes ~100 ms, so a sequential client rarely exceeds 20/s; the limit
    # itself is host-tested (test_command). Only judge it when the rate was really above 20/s.
    t0 = time.time()
    codes = [api.call("POST", "/api/config", {"step": step0})[0] for _ in range(25)]
    rate = len(codes) / max(time.time() - t0, 1e-3)
    if 429 in codes or rate > 21:
        check("more than 20 writes/s -> 429", 429 in codes and codes[0] == 200, "%.1f/s %s" % (rate, codes))
    else:
        print("SKIP  more than 20 writes/s -> 429  (only %.1f writes/s reached; host-tested)" % rate)
    time.sleep(1.1)


def check_wifi(api, skip_scan):
    """WiFi settings (no secrets) and scan semantics; never changes WiFi."""
    st, _, w = api.call("GET", "/api/wifi")
    ok = st == 200 and isinstance(w, dict) and {"mode", "networks", "ap", "name", "staticIp"} <= set(w)
    check("GET /api/wifi", ok, "%d %s" % (st, w))
    if ok:
        check("no keys in /api/wifi", all(set(n) == {"ssid", "key"} for n in w["networks"]) and "password" not in w["ap"])
    st, _, j = api.call("POST", "/api/wifi/confirm", {})
    check("confirm without change -> 400 -221", st == 400 and err(j).startswith("-221,"), "%d %s" % (st, j))
    # D23: mode off takes nothing else. The invalid name keeps older firmware from applying it
    # (-224 there), so this never switches WiFi off.
    st, _, j = api.call("POST", "/api/wifi", {"mode": "off", "name": "Bad_Name"})
    check("WiFi off with another key -> 400 -108", st == 400 and err(j).startswith("-108,"), "%d %s" % (st, j))
    if skip_scan:
        return
    st, _, j = api.call("GET", "/api/wifi/scan")
    if st == 429:
        time.sleep(10.5)
        st, _, j = api.call("GET", "/api/wifi/scan")
    check("scan start -> running", st == 200 and isinstance(j, dict) and j.get("state") == "running", "%d %s" % (st, j))
    state = None
    for _ in range(25):
        time.sleep(1)
        st, _, j = api.call("GET", "/api/wifi/scan")
        state = j.get("state") if isinstance(j, dict) else None
        if state != "running":
            break
    check("scan -> done once", state == "done" and isinstance(j.get("networks"), list), "%s %s" % (st, j))
    st, _, _ = api.call("GET", "/api/wifi/scan")
    check("new scan within 10 s -> 429", st == 429, "HTTP %d" % st)


def ws_connect(api, token=None):
    return ws_open(api.hostport, "/ws?t=" + (api.token if token is None else token), cookie=api.cookie)


def check_ws_push(api, s, buf, step0):
    """State pushed on connect and after a change (restores the step)."""
    op, data, buf = ws_read(s, buf, 3)
    try:
        pushed = json.loads(data.decode()) if op == 0x1 else None
    except ValueError:
        pushed = None
    check("state pushed on connect", isinstance(pushed, dict) and set(pushed) == STATE_KEYS, "op %s" % op)
    api.call("POST", "/api/config", {"step": 1000 if step0 != 1000 else 100})
    op, data, buf = ws_read(s, buf, 3)
    check("state pushed after a change", op == 0x1 and b"\"step\"" in data, "op %s" % op)
    api.call("POST", "/api/config", {"step": step0})
    ws_read(s, buf, 1)


def check_ws_limits(api, first):
    """Three clients at most (a fourth WebSocket and the page get 503), inbound frames <= 512 B."""
    socks = [first] + [ws_connect(api)[0] for _ in range(2)]
    s4, st4, _ = ws_connect(api)
    check("fourth WebSocket -> 503", st4 == 503, "HTTP %d" % st4)
    s4.close()
    st, _, raw = api.raw("GET", "/")
    check("page with 3 clients -> 503 Too many clients", st == 503 and b"Too many clients" in raw, "HTTP %d" % st)
    first.sendall(ws_frame(0x1, b"x" * 600))
    op, data, _ = ws_read(first, b"", 3)
    check("inbound frame > 512 B -> close 1009", op == 0x8 and data[:2] == struct.pack(">H", 1009), "op %s %s" % (op, data[:2]))
    for x in socks:
        x.close()


def check_ws_ping_flood(api):
    """AF18: the UI never pings; a PING flood closes the socket (policy 1008)."""
    time.sleep(0.5)
    f, stf, buf = ws_connect(api)
    closed = None
    if stf == 101:
        for _ in range(8):
            f.sendall(ws_frame(0x9, b"p"))
        end = time.time() + 3
        while time.time() < end and closed is None:
            op, data, buf = ws_read(f, buf, 1)
            if op == 0x8:
                closed = data[:2]
    check("PING flood -> close 1008", closed == struct.pack(">H", 1008), "HTTP %d, close %s" % (stf, closed))
    f.close()


def check_websocket(api, step0):
    """WebSocket (W-S6, W-E10)."""
    s, st, _ = ws_connect(api, "0" * 32)
    check("WebSocket with a wrong token -> 403", st == 403, "HTTP %d" % st)
    s.close()
    s, st, buf = ws_connect(api)
    if not check("WebSocket upgrade -> 101", st == 101, "HTTP %d" % st):
        return
    check_ws_push(api, s, buf, step0)
    check_ws_limits(api, s)
    check_ws_ping_flood(api)


def check_logout(api, password, step0):
    """Logout (S56): {} only, token like every POST, no session needed; the cookie expires and
    the session ends."""
    st, _, j = api.call("POST", "/api/logout", {"x": 1})
    check("POST /api/logout {\"x\":1} -> 400 -108", st == 400 and err(j).startswith("-108,"), "%d %s" % (st, j))
    st, _, _ = api.call("POST", "/api/logout", {}, token=False)
    check("logout without token -> 403", st == 403, "HTTP %d" % st)
    st, hd, _ = api.raw("POST", "/api/logout", {})
    cookie = hd.get("Set-Cookie", "")
    check("logout -> 204 with expired cookie", st == 204 and cookie.startswith("cyd_sid=;") and "Max-Age=0" in cookie,
          "%d %s" % (st, cookie))
    if password:
        st, _, j = api.call("POST", "/api/config", {"step": step0})
        check("write with the old cookie after logout -> 401", st == 401, "%d %s" % (st, j))
        api.cookie = ""
        st, _, _ = api.raw("POST", "/api/logout", {})
        check("logout without session -> 204", st == 204, "HTTP %d" % st)


PASSWORD_ENV = "CYD_WEB_PASSWORD"


def web_password(arg):
    """The web password (AF27): --password without a value asks for it (getpass), otherwise
    the environment variable CYD_WEB_PASSWORD. A value on the command line still works but
    lands in the shell history and the process list, so it is warned about. None = no login."""
    if arg == "":
        return getpass.getpass("Web password: ")
    if arg is not None:
        print("warning: a password on the command line is visible in the shell history and the "
              "process list; use --password without a value or %s" % PASSWORD_ENV, file=sys.stderr)
        return arg
    return os.environ.get(PASSWORD_ENV) or None


def main():
    ap = argparse.ArgumentParser(description="Contract check of the CYD HF Generator web server.")
    ap.add_argument("host", help="host[:port], e.g. cyd-hf-1a2b.local or 127.0.0.1:8080")
    ap.add_argument("--password", nargs="?", const="", metavar="PASSWORD",
                    help="web password, if one is set: without a value it is asked for; "
                         "default: environment variable " + PASSWORD_ENV)
    ap.add_argument("--skip-scan", action="store_true", help="no WiFi scan (it disturbs the link briefly)")
    args = ap.parse_args()
    password = web_password(args.password)
    try:
        api = Api(args.host)
    except ValueError as e:
        ap.error(str(e))

    if not check_page(api):
        return 1
    if password:
        check_login(api, password)
    s0 = check_state(api)
    if s0 is None:
        return 1
    check_guard(api)
    check_rest_errors(api, s0)
    check_writes(api, s0)
    check_wifi(api, args.skip_scan)
    step0 = s0["config"]["step"]
    check_websocket(api, step0)
    check_logout(api, password, step0)

    passed = sum(results)
    print("\n%d PASS, %d FAIL" % (passed, len(results) - passed))
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
