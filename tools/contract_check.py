#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Shared contract vectors (AF34, TEST-1) against the web server: tools/web_mock.py in CI, the
device in MES-07. The same file test/contract/vectors.json is read by the C++ host tests
(test_http_guard, test_rest_json), so a drift between mock, firmware and the contract shows up
on both sides. Python 3 standard library only; one PASS/FAIL line per vector.

    python tools/contract_check.py 127.0.0.1:8099           # the mock
    python tools/contract_check.py cyd-hf-1a2b.local        # the device (no web password)

Only vectors without a password and without a session; the writes are a step change to 100 Hz.
"""
import argparse
import http.client
import json
import os
import sys

import hostport

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_VECTORS = os.path.join(HERE, "..", "test", "contract", "vectors.json")


def answer_code(status, body):
    """0 for a success, else the SCPI code of {"error":"<code>,<text>"}; None if unreadable."""
    if 200 <= status < 300:
        return 0
    try:
        err = json.loads(body.decode("utf-8")).get("error", "")
        return int(err.split(",", 1)[0])
    except (ValueError, AttributeError, UnicodeDecodeError):
        return None


def request(addr, hosthdr, method, path, body=None, headers=None, timeout=5.0):
    conn = http.client.HTTPConnection(addr[0], addr[1], timeout=timeout)
    try:
        conn.putrequest(method, path, skip_host=True, skip_accept_encoding=True)
        hdrs = {"Host": hosthdr}
        hdrs.update(headers or {})
        for k, v in hdrs.items():
            conn.putheader(k, v)
        data = body.encode("utf-8") if body is not None else None
        conn.endheaders(data)
        r = conn.getresponse()
        return r.status, r.read()
    finally:
        conn.close()


def fetch_token(addr, hosthdr):
    status, body = request(addr, hosthdr, "GET", "/api/token", headers={"Sec-Fetch-Site": "same-origin"})
    if status != 200:
        raise RuntimeError("GET /api/token answered %d" % status)
    return json.loads(body.decode("utf-8"))["token"]


def run(target, vectors_path, verbose=False):
    addr = hostport.split(target, 80)
    hosthdr = target.strip()
    with open(vectors_path, encoding="utf-8") as f:
        vectors = json.load(f)["vectors"]
    token = fetch_token(addr, hosthdr)
    failed = 0
    for v in vectors:
        headers = {}
        body = v.get("body")
        if body is not None:
            headers["Content-Type"] = v.get("contentType", "application/json")
            headers["Content-Length"] = str(len(body.encode("utf-8")))
        if v.get("token"):
            headers["X-CYD-Token"] = token
        for k, val in v.get("headers", {}).items():
            headers[k] = val.replace("$TOKEN", token)
        host = headers.pop("Host", hosthdr)
        if body is not None and headers["Content-Length"] != str(len(body.encode("utf-8"))):
            body = None  # an announced length above the limit: refused before the body is read
        try:
            status, data = request(addr, host, v["method"], v["path"], body, headers)
        except (OSError, http.client.HTTPException) as e:
            status, data = -1, str(e).encode()
        code = answer_code(status, data)
        ok = status == v["status"] and code == v["code"]
        name = "%s %s %s" % (v["method"], v["path"], body if body is not None else "")
        print("%s  %s -> %d %s%s" % ("PASS" if ok else "FAIL", name.strip(), v["status"], v["code"],
                                    "" if ok else "  (got %d %s)" % (status, code)))
        if verbose and not ok:
            print("      " + data[:200].decode("utf-8", "replace"))
        failed += not ok
    print("%d of %d vectors passed" % (len(vectors) - failed, len(vectors)))
    return 1 if failed else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("host", help="host[:port], e.g. 127.0.0.1:8099 or cyd-hf-1a2b.local")
    ap.add_argument("--vectors", default=DEFAULT_VECTORS, help="vector file (default test/contract/vectors.json)")
    ap.add_argument("--verbose", action="store_true", help="print the body of a failed answer")
    args = ap.parse_args()
    try:
        return run(args.host, args.vectors, args.verbose)
    except (OSError, ValueError, RuntimeError) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
