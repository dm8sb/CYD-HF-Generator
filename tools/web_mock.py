#!/usr/bin/env python3
"""Mock of the CYD HF Generator web server for trying the web UI in data/www without a device.

Python 3 standard library only. Serves data/www (with {{TOKEN}} replaced), the REST API with an
in-memory state, and a minimal WebSocket push on /ws. It enforces the checks the device will
make (token, content type, origin, body size, unknown keys, types, ranges, rate limits), so the
UI meets the same answers as on the device.

    python tools/web_mock.py                     # http://127.0.0.1:8080/
    python tools/web_mock.py --password secret1  # login required for writes and WiFi
    python tools/web_mock.py --local-only        # Remote outputs: local only (switch-on -> 403)
    python tools/web_mock.py --open-ap           # client sits on the open AP (passwords -> 403)
    python tools/web_mock.py --hw NFO --no-ws --demo --confirm-timeout 20

See data/www/README.md for the API contract. The firmware (S55, net/web_server.cpp) gives the
same answers; where the two differ, the firmware is the reference and this mock follows it.
"""
import argparse
import base64
import copy
import hashlib
import http.server
import json
import math
import os
import re
import secrets
import select
import socket
import struct
import threading
import time
import urllib.parse
from decimal import Decimal, ROUND_HALF_UP

WWW = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "www")
STATIC = {"/app.js": ("app.js", "text/javascript; charset=utf-8"),
          "/style.css": ("style.css", "text/css; charset=utf-8")}
CSP = ("default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; "
       "frame-ancestors 'none'; base-uri 'none'; form-action 'self'")

FREQ_MIN, FREQ_MAX = 8_000, 160_000_000
IQ_MIN, IQ_MAX = 4_730_000, 100_000_000
STEPS = (1, 10, 100, 1000, 10000, 100000, 1000000)
SPREAD = {"down05": (0.005, 0.0), "down15": (0.015, 0.0), "center05": (0.005, 0.005)}
SSC_TEXT = {"down05": "SSC -0.5%", "down15": "SSC -1.5%", "center05": "SSC +/-0.5%"}

E_SYNTAX = "-102,Syntax error"
E_TYPE = "-104,Data type error"
E_KEY = "-108,Parameter not allowed"
E_CONFLICT = "-221,Settings conflict"
E_RANGE = "-222,Data out of range"
E_ILLEGAL = "-224,Illegal parameter value"
E_RATE = "-200,Too many commands"

MAX_BODY = 1024
MAX_WS = 3
WRITES_PER_S = 20
SAVE_DELAY = 5.0
REMOTE_SHOW = 10.0
SCAN_TIME, SCAN_EVERY = 2.0, 10.0
LOGIN_FAILS, LOGIN_LOCK = 5, 30

NAME_RE = re.compile(r"^[a-z0-9]([a-z0-9-]{0,30}[a-z0-9])?$")
IP_RE = re.compile(r"^((25[0-5]|2[0-4]\d|1?\d?\d)\.){3}(25[0-5]|2[0-4]\d|1?\d?\d)$")
FREQ_RE = re.compile(r"^\s*(\d[\d ]*(?:[.,][\d ]*)?|[.,]\d[\d ]*)\s*(hz|khz|mhz|k|m)?\s*$", re.I)

SCAN_LIST = [("Lab", -48, True), ("Lab-Guest", -55, True), ("Workshop", -61, True), ("FRITZ!Box 7590 XY", -67, True),
             ("<img src=x onerror=alert(1)>", -70, True), ("Café über 5G", -72, True), ("open-hotspot", -75, False),
             ('"quoted" & <b>bold</b>', -78, True), ("DIRECT-4F-Printer", -80, True), ("iot", -82, True),
             ("Nachbar", -85, True), ("HamNet", -86, True), ("x" * 32, -88, True), ("Guest", -90, False),
             ("far away", -93, True)]


class ApiError(Exception):
    def __init__(self, status, error, extra=None):
        super().__init__(error)
        self.status, self.error, self.extra = status, error, extra or {}


def is_int(v):
    return isinstance(v, int) and not isinstance(v, bool)


def check_obj(body, types, need_one=False):
    """Unknown keys -> -108, wrong JSON types -> -104 (bool is not an int, 4.0 is not an int),
    need_one: {} -> -109."""
    if not isinstance(body, dict):
        raise ApiError(400, E_TYPE)
    if need_one and not body:
        raise ApiError(400, "-109,Missing parameter")
    for k, v in body.items():
        if k not in types:
            raise ApiError(400, E_KEY)
        t = types[k]
        if t is None:
            continue
        if (t is int and not is_int(v)) or (t is not int and not isinstance(v, t)):
            raise ApiError(400, E_TYPE)


def parse_freq(text):
    """Device formats: '10.7M', '455k', '7 100 000', '10,7 MHz'. Decimals need a unit; no exponents or hex."""
    m = FREQ_RE.match(text) if len(text) <= 32 else None
    if not m:
        raise ApiError(400, E_ILLEGAL)
    num = m.group(1).replace(" ", "").replace(",", ".")
    unit = (m.group(2) or "").lower()
    if num in (".", "") or ("." in num and unit in ("", "hz")):
        raise ApiError(400, E_ILLEGAL)
    scale = {"": 1, "hz": 1, "k": 1000, "khz": 1000, "m": 1000000, "mhz": 1000000}[unit]
    hz = int((Decimal(num) * scale).to_integral_value(rounding=ROUND_HALF_UP))
    if not FREQ_MIN <= hz <= FREQ_MAX:
        raise ApiError(400, E_RANGE)
    return hz


def ip_int(s):
    return struct.unpack(">I", socket.inet_aton(s))[0]


def grid_freq(f):
    """Integer mode: 25 MHz x a / (even divider x R), a = 24..36 (USER_MANUAL section 8)."""
    best = None
    for r in (1, 2, 4, 8, 16, 32, 64, 128):
        for a in range(24, 37):
            vco = 25e6 * a
            d = math.floor(vco / (f * r) / 2) * 2
            for dd in (d, d + 2):
                if 4 <= dd <= 2048:
                    out = vco / (dd * r)
                    if best is None or abs(out - f) < abs(best - f):
                        best = out
    return best


class Model:
    def __init__(self, args):
        self.args = args
        self.lock = threading.RLock()
        self.rev = 1
        self.hw = args.hw
        self.ch = [{"enabled": False, "frequency": 10_000_000}, {"enabled": False, "frequency": 14_000_000}]
        self.ch3 = {"enabled": False, "master": 1, "phase": 90}
        self.cfg = {"drive": 4, "tuning": "fractional", "spread": "off", "offState": "low", "correction": 0,
                    "step": 1000, "backlight": 80, "remoteOutputs": "local" if args.local_only else "allowed"}
        self.save, self.save_due = "saved", 0.0
        self.remote, self.remote_until = None, 0.0
        self.net = {"mode": "ap" if args.open_ap else "client",
                    "networks": [{"ssid": "Lab", "key": "set", "pw": "labpassword"}, {"ssid": "", "key": "none", "pw": ""}],
                    "ap": {"ssid": "CYD-HF-1A2B", "open": bool(args.open_ap), "pw": "" if args.open_ap else "Xk4mP9tR2wQz"},
                    "name": "cyd-hf-1a2b",
                    "staticIp": {"enabled": False, "ip": "", "mask": "", "gateway": "", "dns": ""}}
        self.pending = None  # (deadline, previous network settings)
        self.scan_state, self.scan_started, self.scan_delivered = "idle", -1e9, True
        self.sessions = {}
        self.login_fails, self.login_lock_until, self.login_lock_s = 0, 0.0, LOGIN_LOCK
        self.writes = {}
        self.ws_count = 0
        self.demo_next = time.monotonic() + 5

    # ---- derived values
    def changed(self, save=False, source="web"):
        self.rev += 1
        if save:
            self.save, self.save_due = "pending", time.monotonic() + SAVE_DELAY
        if source == "web":
            self.remote, self.remote_until = "web", time.monotonic() + REMOTE_SHOW

    def spread_of(self, n):
        sp = self.cfg["spread"]
        return sp if sp != "off" and (n == 1 or (n == 3 and self.ch3["master"] == 1)) else "off"

    def actual(self, n, f):
        if self.hw != "CONN":
            return 0.0
        t = self.cfg["tuning"]
        if t == "integer":
            a = grid_freq(f) / (1 + self.cfg["correction"] * 1e-8)
        elif t == "lownoise":
            a = f + ((f * 7919) % 31 - 15) / 10.0
        else:
            a = f + ((f * 7919) % 11 - 5) / 10.0
        sp = self.spread_of(n)
        if sp != "off":  # mean frequency, as SCPI FREQ:ACT? and the Out line
            down, up = SPREAD[sp]
            a *= 1 - (down - up) / 2
        return round(a, 1)

    def modes(self, n):
        m = []
        if self.cfg["offState"] != "low":
            m.append("OFF=High" if self.cfg["offState"] == "high" else "OFF=High-Z")
        sp = self.spread_of(n)
        if sp != "off":
            m.append(SSC_TEXT[sp])
        if self.cfg["tuning"] != "fractional":
            m.append("Low noise" if self.cfg["tuning"] == "lownoise" else "Integer")
        if self.cfg["drive"] != 4:
            m.append("%dmA" % self.cfg["drive"])
        return m

    def iq_range(self):
        lo, hi = IQ_MIN, IQ_MAX
        sp = self.spread_of(3)
        if sp != "off":
            down, up = SPREAD[sp]
            lo = max(lo, math.ceil(600e6 / 127 / (1 - down)))
            hi = min(hi, math.floor(IQ_MAX / (1 + up)))
        return lo, hi

    def ch_status(self, i):
        if self.hw != "CONN":
            return "NOHW"
        return "ON" if self.ch[i]["enabled"] else "OFF"

    def ch3_status(self):
        if not self.ch3["enabled"]:
            return "OFF"
        if self.hw != "CONN":
            return "NOHW"
        m = self.ch[self.ch3["master"] - 1]
        lo, hi = self.iq_range()
        if not m["enabled"] or not lo <= m["frequency"] <= hi:
            return "WAIT"
        return "ON"

    def wifi_status(self):
        n, ap = self.net, self.net["ap"]
        st = {"mode": n["mode"], "connected": False, "ssid": "", "rssi": 0, "ip": "", "name": n["name"],
              "ap": {"active": n["mode"] == "ap", "ssid": ap["ssid"], "open": ap["open"], "stations": 1 if n["mode"] == "ap" else 0}}
        if n["mode"] == "client":
            nets = [x for x in n["networks"] if x["ssid"]]
            if nets:
                st.update(connected=True, ssid=nets[0]["ssid"], rssi=-60,
                          ip=n["staticIp"]["ip"] if n["staticIp"]["enabled"] else "192.168.1.50")
        elif n["mode"] == "ap":
            st["ip"] = "192.168.4.1"
        # Seconds left to confirm a network change (0 = none): visible to every client (S55).
        st["confirmWithin"] = max(0, math.ceil(self.pending[0] - time.monotonic())) if self.pending else 0
        return st

    def state(self):
        with self.lock:
            lo, hi = self.iq_range()
            s3 = self.ch3_status()
            m = self.ch[self.ch3["master"] - 1]
            return {"rev": self.rev, "hw": self.hw, "save": self.save, "remote": self.remote,
                    "ch": [{"n": i + 1, "enabled": c["enabled"], "frequency": c["frequency"],
                            "actual": self.actual(i + 1, c["frequency"]), "status": self.ch_status(i), "modes": self.modes(i + 1)}
                           for i, c in enumerate(self.ch)],
                    "ch3": {"enabled": self.ch3["enabled"], "master": self.ch3["master"], "phase": self.ch3["phase"],
                            "status": s3, "frequency": m["frequency"],
                            "actual": self.actual(3, m["frequency"]) if s3 == "ON" else 0.0, "iqMin": lo, "iqMax": hi},
                    "config": dict(self.cfg), "wifi": self.wifi_status(),
                    "device": {"version": "1.4.0", "passwordSet": bool(self.args.password)}}

    def wifi_settings(self):
        n = self.net
        return {"mode": n["mode"], "networks": [{"ssid": x["ssid"], "key": x["key"]} for x in n["networks"]],
                "ap": {"ssid": n["ap"]["ssid"], "open": n["ap"]["open"], "passwordSet": bool(n["ap"]["pw"])},
                "name": n["name"], "staticIp": dict(n["staticIp"])}

    # ---- writes
    def switch_on_allowed(self, now_on, want):
        if want and not now_on and self.cfg["remoteOutputs"] == "local":
            raise ApiError(403, "-221,Remote outputs: local only")

    def post_channel(self, n, b):
        check_obj(b, {"enabled": bool, "frequency": int, "frequencyText": str}, need_one=True)
        if "frequency" in b and "frequencyText" in b:
            raise ApiError(400, "-108,frequency or frequencyText, not both")
        f = None
        if "frequency" in b:
            f = b["frequency"]
            if not FREQ_MIN <= f <= FREQ_MAX:
                raise ApiError(400, E_RANGE)
        elif "frequencyText" in b:
            f = parse_freq(b["frequencyText"])
        c = self.ch[n - 1]
        if "enabled" in b:
            self.switch_on_allowed(c["enabled"], b["enabled"])
        save = f is not None and f != c["frequency"]
        if f is not None:
            c["frequency"] = f
        if "enabled" in b:
            c["enabled"] = b["enabled"]
        self.changed(save)

    def post_ch3(self, b):
        check_obj(b, {"enabled": bool, "master": int, "phase": int}, need_one=True)
        if "master" in b and b["master"] not in (1, 2):
            raise ApiError(400, E_ILLEGAL)
        if "phase" in b and b["phase"] not in (0, 90, 180, 270):
            raise ApiError(400, E_ILLEGAL)
        if "enabled" in b:
            self.switch_on_allowed(self.ch3["enabled"], b["enabled"])
        save = any(k in b and b[k] != self.ch3[k] for k in ("master", "phase"))
        self.ch3.update(b)
        self.changed(save)

    def post_config(self, b):
        if isinstance(b, dict) and "remoteOutputs" in b:
            raise ApiError(403, "-221,Remote outputs can only be changed at the device")
        check_obj(b, {"drive": int, "tuning": str, "spread": str, "offState": str, "correction": int, "step": int, "backlight": int}, need_one=True)
        enums = {"drive": (2, 4, 6, 8), "tuning": ("fractional", "lownoise", "integer"),
                 "spread": ("off", "down05", "down15", "center05"), "offState": ("low", "high", "hiz"), "step": STEPS}
        for k, allowed in enums.items():
            if k in b and b[k] not in allowed:
                raise ApiError(400, E_ILLEGAL)
        if "correction" in b and abs(b["correction"]) > 5000:
            raise ApiError(400, E_RANGE)
        if "backlight" in b and not 10 <= b["backlight"] <= 100:
            raise ApiError(400, E_RANGE)
        tuning = b.get("tuning", self.cfg["tuning"])
        if tuning == "integer" and b.get("spread", "off") != "off":
            raise ApiError(400, E_CONFLICT)
        new = dict(self.cfg, **b)
        if tuning == "integer":
            new["spread"] = "off"  # locked in Integer, as at the device
        save = new != self.cfg
        self.cfg = new
        self.changed(save)

    def post_wifi(self, b, open_ap_client):
        check_obj(b, {"mode": str, "networks": list, "delete": list, "name": str, "staticIp": dict, "ap": dict}, need_one=True)
        if self.pending:
            raise ApiError(400, "-221,A network change waits for confirmation")
        has_pw = any(isinstance(x, dict) and "password" in x for x in b.get("networks", [])) or "password" in b.get("ap", {})
        if has_pw and open_ap_client:
            raise ApiError(403, "-221,Passwords cannot be sent over the open access point")
        if "mode" in b and b["mode"] not in ("off", "client", "ap"):
            raise ApiError(400, E_ILLEGAL)
        if len(b.get("networks", [])) > 2 or len(b.get("delete", [])) > 2:
            raise ApiError(400, E_RANGE)
        for x in b.get("networks", []):
            check_obj(x, {"slot": int, "ssid": str, "password": str})
            if x.get("slot") not in (0, 1) or not 1 <= len(x.get("ssid", "").encode()) <= 32:
                raise ApiError(400, E_ILLEGAL)
            pw = x.get("password")
            if pw is not None and pw != "" and not 8 <= len(pw) <= 63:
                raise ApiError(400, E_ILLEGAL)
        for s in b.get("delete", []):
            if not is_int(s):
                raise ApiError(400, E_TYPE)
            if s not in (0, 1):
                raise ApiError(400, E_ILLEGAL)
        if "name" in b and not NAME_RE.match(b["name"]):
            raise ApiError(400, E_ILLEGAL)
        si = b.get("staticIp")
        if si is not None:
            check_obj(si, {"enabled": bool, "ip": str, "mask": str, "gateway": str, "dns": str})
            si = dict(self.net["staticIp"], **si)
            if si["enabled"]:
                for k in ("ip", "mask", "gateway"):
                    if not IP_RE.match(si[k]):
                        raise ApiError(400, E_ILLEGAL)
                if si["dns"] and not IP_RE.match(si["dns"]):
                    raise ApiError(400, E_ILLEGAL)
                ip, mask, gw = ip_int(si["ip"]), ip_int(si["mask"]), ip_int(si["gateway"])
                inv = ~mask & 0xFFFFFFFF
                if inv & (inv + 1) or mask == 0:
                    raise ApiError(400, E_ILLEGAL)  # mask not contiguous
                if ip == gw or ip & mask != gw & mask or ip & inv in (0, inv) or gw & inv in (0, inv):
                    raise ApiError(400, E_CONFLICT)
        ap = b.get("ap")
        if ap is not None:
            check_obj(ap, {"open": bool, "password": str})
            if "password" in ap and not 8 <= len(ap["password"]) <= 63:
                raise ApiError(400, E_ILLEGAL)
        prev = copy.deepcopy(self.net)
        n = self.net
        if "mode" in b:
            n["mode"] = b["mode"]
        for s in b.get("delete", []):
            n["networks"][s] = {"ssid": "", "key": "none", "pw": ""}
        for x in b.get("networks", []):
            slot = n["networks"][x["slot"]]
            if "password" in x:
                slot.update(pw=x["password"], key="set" if x["password"] else "open")
            elif x["ssid"] != slot["ssid"]:
                slot.update(pw="", key="open")
            slot["ssid"] = x["ssid"]
        if "name" in b:
            n["name"] = b["name"]
        if si is not None:
            n["staticIp"] = si
        if ap is not None:
            n["ap"].update({k: v for k, v in ap.items() if k == "open"})
            if "password" in ap:
                n["ap"]["pw"] = ap["password"]
        self.changed()
        if n["mode"] == "off":
            # Final, like the device (ARBEITSPLAN 3.6 point 2): no new connection to confirm over.
            print("[mock] network change: WiFi off (final, nothing to confirm)")
            return 0
        self.pending = (time.monotonic() + self.args.confirm_timeout, prev)
        print("[mock] network change applied, confirm within %d s" % self.args.confirm_timeout)
        return self.args.confirm_timeout

    def confirm_wifi(self, b):
        check_obj(b, {})
        if not self.pending:
            raise ApiError(400, "-221,Nothing to confirm")
        self.pending = None
        self.changed()
        print("[mock] network change confirmed")

    def scan(self):
        now = time.monotonic()
        if self.scan_state == "running":
            if now - self.scan_started < SCAN_TIME:
                return {"state": "running", "networks": []}
            self.scan_state, self.scan_delivered = "done", True
            return {"state": "done", "networks": [{"ssid": s, "rssi": r, "secure": sec} for s, r, sec in SCAN_LIST[:15]]}
        if now - self.scan_started < SCAN_EVERY:
            raise ApiError(429, "-200,Scan at most every 10 s")
        self.scan_state, self.scan_started, self.scan_delivered = "running", now, False
        return {"state": "running", "networks": []}

    def tick(self):
        """Timers: autosave, REMOTE indicator, network fallback, demo changes."""
        with self.lock:
            now = time.monotonic()
            if self.save == "pending" and now >= self.save_due:
                self.save = "saved"
                self.changed(source=None)
            if self.remote and now >= self.remote_until:
                self.remote = None
                self.changed(source=None)
            if self.pending and now >= self.pending[0]:
                self.net = self.pending[1]
                self.pending = None
                self.changed(source=None)
                print("[mock] network change not confirmed: previous settings restored")
            if self.args.demo and now >= self.demo_next:
                self.demo_next = now + 4
                c = self.ch[1]
                c["frequency"] = c["frequency"] + 1000 if c["frequency"] < 14_100_000 else 14_000_000
                self.changed(save=True, source=None)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "cyd-mock"
    model = None
    token = ""

    def log_message(self, fmt, *a):
        if self.server.verbose:
            super().log_message(fmt, *a)

    # ---- responses
    def send(self, status, body=b"", ctype="application/json", headers=None):
        if isinstance(body, (dict, list)):
            body = json.dumps(body, separators=(",", ":")).encode()
        elif isinstance(body, str):
            body = body.encode()
        self.send_response(status)
        if body:
            self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Security-Policy", CSP)
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cache-Control", "no-store" if self.path.startswith("/api/") else "no-cache")
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def error(self, e):
        self.send(e.status, dict({"error": e.error}, **e.extra))

    # ---- checks shared by all requests
    def host_ok(self):
        host = (self.headers.get("Host") or "").rsplit(":", 1)[0].strip("[]").lower()
        name = self.model.net["name"]
        allowed = {"localhost", "127.0.0.1", self.connection.getsockname()[0], name, name + ".local", self.server.bind_host}
        return host in allowed

    def own_origin(self):
        return "http://" + (self.headers.get("Host") or "")

    def session_ok(self):
        if not self.server.password:
            return True
        cookie = self.headers.get("Cookie") or ""
        m = re.search(r"(?:^|;\s*)cyd_sid=([0-9a-f]{32})(?:;|$)", cookie)
        exp = self.model.sessions.get(m.group(1)) if m else None
        return exp is not None and exp > time.monotonic()

    def open_ap_client(self):
        return self.server.open_ap

    # ---- methods
    def do_OPTIONS(self):
        self.send(405, {"error": "-200,OPTIONS not allowed"}, headers={"Allow": "GET, POST"})

    do_PUT = do_DELETE = do_PATCH = do_OPTIONS

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        if len(self.path) > 256:
            return self.send(414, {"error": "-200,URL too long"})
        if not self.host_ok():
            return self.send(421, {"error": "-200,Host not allowed"})
        url = urllib.parse.urlsplit(self.path)
        if url.path == "/ws":
            return self.websocket(url)
        if url.path in ("/", "/index.html"):
            if self.model.ws_count >= MAX_WS:
                return self.send(503, "<!DOCTYPE html><title>CYD HF Generator</title><p>Too many clients (max. 3)</p>",
                                 "text/html; charset=utf-8")
            with open(os.path.join(WWW, "index.html"), encoding="utf-8") as f:
                html = f.read().replace("{{TOKEN}}", self.token)
            return self.send(200, html, "text/html; charset=utf-8")
        if url.path in STATIC:
            name, ctype = STATIC[url.path]
            with open(os.path.join(WWW, name), "rb") as f:
                return self.send(200, f.read(), ctype)
        m = self.model
        try:
            with m.lock:
                if url.path == "/api/token":
                    return self.send(200, {"token": self.token})
                if url.path == "/api/state":
                    if self.server.login_all and not self.session_ok():
                        raise ApiError(401, "-200,Login required")
                    return self.send(200, m.state())
                if url.path in ("/api/wifi", "/api/wifi/scan"):
                    if not self.session_ok():
                        raise ApiError(401, "-200,Login required")
                    return self.send(200, m.wifi_settings() if url.path == "/api/wifi" else m.scan())
        except ApiError as e:
            return self.error(e)
        self.send(404, {"error": "-200,Not found"})

    def do_POST(self):
        if not self.host_ok():
            return self.send(421, {"error": "-200,Host not allowed"})
        m = self.model
        try:
            if "chunked" in (self.headers.get("Transfer-Encoding") or "").lower():
                raise ApiError(415, "-200,Transfer-Encoding not allowed")
            length = int(self.headers.get("Content-Length") or 0)
            if length > MAX_BODY:
                self.close_connection = True
                raise ApiError(413, "-200,Body too large")
            raw = self.rfile.read(length) if length else b""
            ctype = (self.headers.get("Content-Type") or "").split(";")[0].strip().lower()
            if ctype != "application/json":
                raise ApiError(415, "-200,Content-Type must be application/json")
            origin = self.headers.get("Origin")
            if origin and origin != self.own_origin():
                raise ApiError(403, "-200,Foreign origin")
            if (self.headers.get("Sec-Fetch-Site") or "").lower() == "cross-site":
                raise ApiError(403, "-200,Cross-site request")
            if not secrets.compare_digest(self.headers.get("X-CYD-Token") or "", self.token):
                raise ApiError(403, "-200,Invalid token: reload the page")
            try:
                body = json.loads(raw.decode("utf-8")) if raw else None
            except (ValueError, UnicodeDecodeError):
                raise ApiError(400, E_SYNTAX)
            path = urllib.parse.urlsplit(self.path).path
            if path == "/api/login":
                return self.login(body)
            if path == "/api/logout":
                return self.logout(body)
            if not self.session_ok():
                raise ApiError(401, "-200,Login required")
            with m.lock:
                self.rate_limit()
                if body is None:
                    raise ApiError(400, E_SYNTAX)
                ch = re.fullmatch(r"/api/channel/(\d+)", path)
                if ch and ch.group(1) in ("1", "2"):
                    m.post_channel(int(ch.group(1)), body)
                elif path == "/api/ch3":
                    m.post_ch3(body)
                elif path == "/api/config":
                    m.post_config(body)
                elif path == "/api/save":
                    check_obj(body, {})
                    if m.save in ("pending", "failed"):
                        m.save = "saved"
                        m.changed()
                    print("[mock] save")
                elif path == "/api/identify":
                    check_obj(body, {})
                    print("[mock] IDENTIFY: backlight and LED blink for 5 s")
                elif path == "/api/wifi":
                    return self.send(202, {"confirmWithin": m.post_wifi(body, self.open_ap_client())})
                elif path == "/api/wifi/confirm":
                    m.confirm_wifi(body)
                    return self.send(204)
                else:
                    raise ApiError(404, "-200,Not found")
                return self.send(200, m.state())
        except ApiError as e:
            return self.error(e)

    def rate_limit(self):
        now = time.monotonic()
        q = [t for t in self.model.writes.get(self.client_address[0], []) if now - t < 1.0]
        if len(q) >= WRITES_PER_S:
            raise ApiError(429, E_RATE)
        q.append(now)
        self.model.writes[self.client_address[0]] = q

    def login(self, body):
        m = self.model
        check_obj(body, {"password": str})
        if "password" not in body:
            raise ApiError(400, "-109,Missing parameter")
        now = time.monotonic()
        with m.lock:
            if now < m.login_lock_until:
                raise ApiError(429, "-200,Too many failed logins", {"retryAfter": math.ceil(m.login_lock_until - now)})
            if self.server.password and not secrets.compare_digest(body["password"], self.server.password):
                m.login_fails += 1
                if m.login_fails >= LOGIN_FAILS:
                    m.login_fails = 0
                    m.login_lock_until = now + m.login_lock_s
                    m.login_lock_s *= 2
                raise ApiError(401, "-200,Wrong password")
            m.login_fails, m.login_lock_s = 0, LOGIN_LOCK
            if not self.server.password:
                return self.send(204)  # no password: nothing to log in to, no cookie
            sid = secrets.token_hex(16)
            m.sessions[sid] = now + 12 * 3600
        self.send(204, headers={"Set-Cookie": "cyd_sid=%s; Path=/; Max-Age=43200; HttpOnly; SameSite=Strict" % sid})

    def logout(self, body):
        """POST /api/logout {}: ends this cookie's session (also without one) and expires the cookie."""
        if body is None:
            raise ApiError(400, E_SYNTAX)
        check_obj(body, {})
        m = re.search(r"(?:^|;\s*)cyd_sid=([0-9a-f]{32})(?:;|$)", self.headers.get("Cookie") or "")
        with self.model.lock:
            if m:
                self.model.sessions.pop(m.group(1), None)
        self.send(204, headers={"Set-Cookie": "cyd_sid=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict"})

    # ---- WebSocket (RFC 6455, push only)
    def websocket(self, url):
        m = self.model
        q = urllib.parse.parse_qs(url.query)
        key = self.headers.get("Sec-WebSocket-Key")
        origin = self.headers.get("Origin")
        if self.server.no_ws:
            return self.send(404, {"error": "-200,Not found"})
        if (self.headers.get("Upgrade") or "").lower() != "websocket" or not key:
            return self.send(400, {"error": "-200,WebSocket upgrade expected"})
        if q.get("t", [""])[0] != self.token or (origin and origin != self.own_origin()):
            return self.send(403, {"error": "-200,Forbidden"})
        if self.server.login_all and not self.session_ok():
            return self.send(401, {"error": "-200,Login required"})
        with m.lock:
            if m.ws_count >= MAX_WS:
                return self.send(503, {"error": "-200,Too many clients (max. 3)"})
            m.ws_count += 1
        try:
            accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
            self.send_response(101, "Switching Protocols")
            self.send_header("Upgrade", "websocket")
            self.send_header("Connection", "Upgrade")
            self.send_header("Sec-WebSocket-Accept", accept)
            self.end_headers()
            self.ws_loop()
        finally:
            with m.lock:
                m.ws_count -= 1
            self.close_connection = True

    def ws_send(self, op, data=b""):
        n = len(data)
        hdr = bytes([0x80 | op]) + (bytes([n]) if n < 126 else bytes([126]) + struct.pack(">H", n))
        self.connection.sendall(hdr + data)

    def ws_read(self, n):
        buf = b""
        while len(buf) < n:
            part = self.connection.recv(n - len(buf))
            if not part:
                raise ConnectionError("closed")
            buf += part
        return buf

    def ws_loop(self):
        sock, m = self.connection, self.model
        sock.settimeout(5)
        last_rev, last_ping, last_pong = None, time.monotonic(), time.monotonic()
        while True:
            with m.lock:
                rev = m.rev
                st = json.dumps(m.state(), separators=(",", ":")).encode() if rev != last_rev else None
            if st:
                self.ws_send(0x1, st)
                last_rev = rev
            now = time.monotonic()
            if now - last_ping >= 15:
                self.ws_send(0x9, b"cyd")
                last_ping = now
            if now - last_pong > 30:
                self.ws_send(0x8, struct.pack(">H", 1001))
                return
            if not select.select([sock], [], [], 0.2)[0]:
                continue
            b1, b2 = self.ws_read(2)
            op, ln = b1 & 0x0F, b2 & 0x7F
            if ln == 126:
                ln = struct.unpack(">H", self.ws_read(2))[0]
            elif ln == 127:
                ln = struct.unpack(">Q", self.ws_read(8))[0]
            if not b2 & 0x80 or ln > 512:  # client frames are masked; at most 512 B (W-S6)
                self.ws_send(0x8, struct.pack(">H", 1002 if not b2 & 0x80 else 1009))
                return
            mask = self.ws_read(4)
            data = bytes(c ^ mask[i % 4] for i, c in enumerate(self.ws_read(ln)))
            if op == 0x8:
                self.ws_send(0x8, data[:2])
                return
            if op == 0x9:
                self.ws_send(0xA, data)
            elif op == 0xA:
                last_pong = time.monotonic()
            # Text and binary frames are ignored: the socket is push only.


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    ap = argparse.ArgumentParser(description="Mock of the CYD HF Generator web server (data/www + REST + WebSocket).")
    ap.add_argument("--host", default="127.0.0.1", help="bind address (0.0.0.0 for phones in the LAN)")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--password", default="", help="web password: login required for writes and WiFi settings")
    ap.add_argument("--login-all", action="store_true", help="with --password: also /api/state and /ws need the login")
    ap.add_argument("--local-only", action="store_true", help="Remote outputs: local only (switch-on answers 403)")
    ap.add_argument("--open-ap", action="store_true", help="the client sits on the open AP: password fields answer 403")
    ap.add_argument("--hw", default="CONN", choices=("CONN", "NFO", "BLOC", "UNKN"), help="SI5351 link state")
    ap.add_argument("--no-ws", action="store_true", help="no WebSocket (the UI falls back to polling)")
    ap.add_argument("--confirm-timeout", type=int, default=120, help="seconds to confirm a network change")
    ap.add_argument("--token", default="", help="fixed token (default: random per start, like per boot)")
    ap.add_argument("--demo", action="store_true", help="change CH2 every 4 s as if someone used the touch screen")
    ap.add_argument("--verbose", action="store_true", help="log every request")
    args = ap.parse_args()
    if args.password and len(args.password) < 8:
        ap.error("the web password needs at least 8 characters (W-S13)")

    Handler.model = Model(args)
    Handler.token = args.token or secrets.token_hex(16)
    srv = Server((args.host, args.port), Handler)
    srv.bind_host, srv.password, srv.login_all = args.host, args.password, args.login_all and bool(args.password)
    srv.open_ap, srv.no_ws, srv.verbose = args.open_ap, args.no_ws, args.verbose

    def ticker():
        while True:
            Handler.model.tick()
            time.sleep(0.25)

    threading.Thread(target=ticker, daemon=True).start()
    host = "127.0.0.1" if args.host in ("0.0.0.0", "") else args.host
    print("CYD HF Generator mock: open http://%s:%d/" % (host, srv.server_address[1]), flush=True)
    print("token %s%s" % (Handler.token, ", password set" if args.password else ""), flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
