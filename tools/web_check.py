#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Contract check of the web server (S55, S56): REST, guard answers, WebSocket push, scan, login, logout.

Python 3 standard library only. Runs against the device or tools/web_mock.py and prints one
PASS/FAIL line per check, like device_check.py does for SCPI. It changes the step and the CH2
frequency briefly and restores them; it never switches an output on and never changes WiFi.

    python tools/web_check.py cyd-hf-1a2b.local            # the device (port 80)
    python tools/web_check.py 127.0.0.1:8080               # the mock
    python tools/web_check.py 192.168.1.50 --password secret123
    python tools/web_check.py 192.168.4.1 --skip-scan      # over the AP
"""
import argparse
import base64
import json
import os
import re
import socket
import struct
import sys
import time
import urllib.error
import urllib.request

TOKEN_RE = re.compile(r'<meta\s+name="cyd-token"\s+content="([0-9a-f]{32})"', re.I)
STATE_KEYS = {"rev", "hw", "save", "remote", "ch", "ch3", "config", "wifi", "device"}

results = []


def check(name, ok, detail=""):
    results.append(ok)
    print("%s  %s%s" % ("PASS" if ok else "FAIL", name, ("  (" + detail + ")") if detail and not ok else ""))
    return ok


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k):
        return None


class Api:
    def __init__(self, hostport):
        self.hostport = hostport
        self.base = "http://" + hostport
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
def ws_open(hostport, path, timeout=5):
    host, _, port = hostport.partition(":")
    s = socket.create_connection((host, int(port or 80)), timeout=timeout)
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall(("GET %s HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
               "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n" % (path, hostport, key)).encode())
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


def main():
    ap = argparse.ArgumentParser(description="Contract check of the CYD HF Generator web server.")
    ap.add_argument("host", help="host[:port], e.g. cyd-hf-1a2b.local or 127.0.0.1:8080")
    ap.add_argument("--password", help="web password, if one is set")
    ap.add_argument("--skip-scan", action="store_true", help="no WiFi scan (it disturbs the link briefly)")
    args = ap.parse_args()
    api = Api(args.host)

    # Page and assets (W-S7).
    st, hd, raw = api.raw("GET", "/")
    page = raw.decode("utf-8", "replace")
    m = TOKEN_RE.search(page)
    check("GET / 200 with per-boot token", st == 200 and m is not None, "HTTP %d" % st)
    if not m:
        return 1
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

    # Login first if a password is set.
    if args.password:
        st, _, j = api.call("POST", "/api/config", {"step": 1000})
        check("write without login -> 401", st == 401 and "Login required" in err(j), "%d %s" % (st, j))
        st, _, j = api.call("POST", "/api/login", {"password": args.password + "x"})
        check("wrong password -> 401", st == 401 and "Wrong password" in err(j), "%d %s" % (st, j))
        t0 = time.time()
        st, hd, _ = api.raw("POST", "/api/login", {"password": args.password})
        cookie = hd.get("Set-Cookie", "")
        check("login -> 204 with HttpOnly SameSite=Strict cookie",
              st == 204 and "HttpOnly" in cookie and "SameSite=Strict" in cookie, "%d %s" % (st, cookie))
        print("      login took %.2f s" % (time.time() - t0))
        api.cookie = cookie.split(";", 1)[0]

    # State (contract).
    st, hd, s0 = api.call("GET", "/api/state")
    ok = st == 200 and isinstance(s0, dict) and set(s0) == STATE_KEYS
    check("GET /api/state contract keys", ok, "%d %s" % (st, sorted(s0) if isinstance(s0, dict) else s0))
    if not ok:
        return 1
    check("/api/* no-store", hd.get("Cache-Control") == "no-store")
    check("wifi.confirmWithin and device.passwordSet",
          isinstance(s0["wifi"].get("confirmWithin"), int) and isinstance(s0["device"].get("passwordSet"), bool))
    check("state has no MAC/commit", "mac" not in json.dumps(s0).lower() and "commit" not in json.dumps(s0).lower())
    st, _, j = api.call("GET", "/api/token")
    check("GET /api/token", st == 200 and isinstance(j, dict) and j.get("token") == api.token, "%d %s" % (st, j))

    # Guard answers (W-S4, W-S5, W-S10).
    st, _, j = api.call("POST", "/api/config", {"step": 1000}, token=False)
    check("POST without token -> 403 reload text", st == 403 and err(j) == "-200,Invalid token: reload the page", "%d %s" % (st, j))
    st, _, _ = api.call("POST", "/api/config", b"step=1", ctype="text/plain")
    check("POST text/plain -> 415", st == 415, "HTTP %d" % st)
    st, _, _ = api.call("POST", "/api/config", {"step": 1000}, headers={"Origin": "http://evil.example"})
    check("foreign Origin -> 403", st == 403, "HTTP %d" % st)
    st, _, _ = api.call("POST", "/api/config", {"step": 1000}, headers={"Sec-Fetch-Site": "cross-site"})
    check("Sec-Fetch-Site cross-site -> 403", st == 403, "HTTP %d" % st)
    st, _, _ = api.call("OPTIONS", "/api/state")
    check("OPTIONS -> 405", st == 405, "HTTP %d" % st)
    st, _, _ = api.call("GET", "/api/state", headers={"Host": "evil.example"})
    check("foreign Host -> 421 (or captive 302 over the AP)", st in (421, 302), "HTTP %d" % st)
    st, _, _ = api.call("POST", "/api/config", b"{" + b" " * 1100 + b"}")
    check("body > 1 KB -> 413", st == 413, "HTTP %d" % st)
    st, _, _ = api.call("GET", "/api/state?" + "a" * 300)
    check("URL > 256 B -> 414", st == 414, "HTTP %d" % st)

    # REST errors (ARBEITSPLAN 3.6 point 5, 9, 11).
    cases = [("/api/config", b"{\"drive\":", "-102"), ("/api/config", {"drive": "4"}, "-104"), ("/api/config", {"drive": 4.5}, "-104"),
             ("/api/config", {"x": 1}, "-108"), ("/api/config", {"drive": 5}, "-224"), ("/api/config", {"correction": 99999}, "-222"),
             ("/api/channel/1", {"frequency": 1}, "-222"), ("/api/channel/1", {"frequency": 7000000, "frequencyText": "7M"}, "-108"),
             ("/api/channel/1", {"frequencyText": "abc"}, "-224"), ("/api/ch3", {"phase": 45}, "-224"), ("/api/ch3", {"master": 3}, "-224"),
             ("/api/config", {}, "-109")]
    for path, body, code in cases:
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

    # Writes answer with the state that holds them.
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

    # WiFi settings (no secrets) and scan semantics.
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
    if not args.skip_scan:
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

    # WebSocket (W-S6, W-E10).
    s, st, _ = ws_open(args.host, "/ws?t=" + "0" * 32)
    check("WebSocket with a wrong token -> 403", st == 403, "HTTP %d" % st)
    s.close()
    socks = []
    s, st, buf = ws_open(args.host, "/ws?t=" + api.token)
    ok = check("WebSocket upgrade -> 101", st == 101, "HTTP %d" % st)
    if ok:
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
        socks.append(s)
        for _ in range(2):
            s2, st2, _ = ws_open(args.host, "/ws?t=" + api.token)
            socks.append(s2)
        s4, st4, _ = ws_open(args.host, "/ws?t=" + api.token)
        check("fourth WebSocket -> 503", st4 == 503, "HTTP %d" % st4)
        s4.close()
        st, _, raw = api.raw("GET", "/")
        check("page with 3 clients -> 503 Too many clients", st == 503 and b"Too many clients" in raw, "HTTP %d" % st)
        big = socks[0]
        big.sendall(ws_frame(0x1, b"x" * 600))
        op, data, _ = ws_read(big, b"", 3)
        check("inbound frame > 512 B -> close 1009", op == 0x8 and data[:2] == struct.pack(">H", 1009), "op %s %s" % (op, data[:2]))
        for x in socks:
            x.close()

    # Logout (S56): {} only, token like every POST, no session needed; the cookie expires and
    # the session ends.
    st, _, j = api.call("POST", "/api/logout", {"x": 1})
    check("POST /api/logout {\"x\":1} -> 400 -108", st == 400 and err(j).startswith("-108,"), "%d %s" % (st, j))
    st, _, _ = api.call("POST", "/api/logout", {}, token=False)
    check("logout without token -> 403", st == 403, "HTTP %d" % st)
    st, hd, _ = api.raw("POST", "/api/logout", {})
    cookie = hd.get("Set-Cookie", "")
    check("logout -> 204 with expired cookie", st == 204 and cookie.startswith("cyd_sid=;") and "Max-Age=0" in cookie,
          "%d %s" % (st, cookie))
    if args.password:
        st, _, j = api.call("POST", "/api/config", {"step": step0})
        check("write with the old cookie after logout -> 401", st == 401, "%d %s" % (st, j))
        api.cookie = ""
        st, _, _ = api.raw("POST", "/api/logout", {})
        check("logout without session -> 204", st == 204, "HTTP %d" % st)

    passed = sum(results)
    print("\n%d PASS, %d FAIL" % (passed, len(results) - passed))
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
