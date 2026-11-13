#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Minimal REST example for the CYD HF generator (urllib only, no requests).

The REST API is in the firmware since S55 (net/web_server.cpp); tools/web_mock.py
answers the same way. tools/web_check.py checks the whole contract.

What it does:
  1. GET /                      -> extract the per-boot token from
                                   <meta name="cyd-token" content="...">
  2. (optional) POST /api/login -> only with --password; the session cookie
                                   is kept in a cookie jar
  3. GET /api/state             -> print the state
  4. POST /api/channel/1        {"frequencyText": "7.1 MHz"}, then
                                {"enabled": true}
  5. GET /api/state again, then switch CH1 off

POST requests need "Content-Type: application/json" and the header
"X-CYD-Token". Errors come back as HTTP 400 with {"error": "-222,Data out of
range"} (same codes as SCPI); they are printed, not raised.

WARNING: step 4 switches CH1 on (briefly). The generator outputs RF; do not
connect an antenna without a filter.

Usage:
    python rest_example.py cyd-hf-1a2b.local
    python rest_example.py 192.168.1.50 --password secret
"""
import argparse
import http.cookiejar
import json
import re
import sys
import urllib.error
import urllib.request

TOKEN_RE = re.compile(r'<meta\s+name="cyd-token"\s+content="([^"]*)"', re.I)


class Api:
    def __init__(self, base):
        self.base = base.rstrip("/")
        self.token = None
        jar = http.cookiejar.CookieJar()
        self.opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

    def request(self, method, path, body=None):
        """Return (http status, parsed JSON or text). HTTP errors do not raise."""
        data = None
        headers = {}
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        if method == "POST":
            headers["X-CYD-Token"] = self.token or ""
        req = urllib.request.Request(self.base + path, data=data, headers=headers, method=method)
        try:
            with self.opener.open(req, timeout=5) as resp:
                status, raw = resp.status, resp.read()
        except urllib.error.HTTPError as e:
            status, raw = e.code, e.read()
        text = raw.decode("utf-8", "replace")
        try:
            return status, json.loads(text)
        except ValueError:
            return status, text

    def fetch_token(self):
        status, page = self.request("GET", "/")
        m = TOKEN_RE.search(page if isinstance(page, str) else "")
        if status != 200 or not m:
            raise RuntimeError("no cyd-token meta tag found (HTTP %s)" % status)
        self.token = m.group(1)


def show(label, status, payload):
    print("%s -> HTTP %d" % (label, status))
    print("   ", json.dumps(payload) if not isinstance(payload, str) else payload)


def main():
    ap = argparse.ArgumentParser(description="Minimal REST example for the CYD HF generator.")
    ap.add_argument("host", help="host name or IP, e.g. cyd-hf-1a2b.local (port 80)")
    ap.add_argument("--password", help="web password, if one is set on the device")
    args = ap.parse_args()
    host = args.host if args.host.startswith("http") else "http://" + args.host

    api = Api(host)
    api.fetch_token()
    print("Token:", api.token)

    if args.password is not None:
        show("POST /api/login", *api.request("POST", "/api/login", {"password": args.password}))

    show("GET /api/state", *api.request("GET", "/api/state"))
    try:
        show("POST /api/channel/1 frequencyText",
             *api.request("POST", "/api/channel/1", {"frequencyText": "7.1 MHz"}))
        show("POST /api/channel/1 enabled",
             *api.request("POST", "/api/channel/1", {"enabled": True}))
        show("GET /api/state", *api.request("GET", "/api/state"))
    finally:
        show("POST /api/channel/1 off", *api.request("POST", "/api/channel/1", {"enabled": False}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
