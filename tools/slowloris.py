#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Slow-request load for MESSPLAN M-210 (A-710, AF39): only against your own device in your own lab.

Opens N connections (default 30) to the web server, sends a request line and one header line,
then a further header byte every few seconds, and completes the requests after --hold seconds
(default 10). For each connection it records whether it was accepted, when the server closed it
and what it answered. The device takes at most 4 HTTP connections, refuses more and closes a
request that is not complete after 10 s (pre-filter deadline); it must stay usable at the touch
screen meanwhile (M-210). Python 3 standard library only.

    python tools/slowloris.py 192.168.1.50                  # 30 connections, complete after 10 s
    python tools/slowloris.py 127.0.0.1:8099 -n 10 --hold 3  # against tools/web_mock.py

Exit code 0 when every connection got an answer or was closed by the server, 1 when one was
still open without an answer at the end, 2 when the target cannot be reached at all.
"""
import argparse
import select
import socket
import sys
import time

import hostport


class Conn:
    def __init__(self, n):
        self.n = n
        self.sock = None
        self.error = ""
        self.closed_at = None   # seconds after the start when the server closed
        self.answer = b""

    def status(self):
        line = self.answer.split(b"\r\n", 1)[0].decode("latin-1", "replace")
        parts = line.split(" ")
        return int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else None


def poll(conns, t0, timeout=0.0):
    """Reads what the server sent; marks closed connections."""
    socks = [c.sock for c in conns if c.sock and c.closed_at is None]
    if not socks:
        return
    readable, _, _ = select.select(socks, [], [], timeout)
    for c in conns:
        if c.sock in readable:
            try:
                data = c.sock.recv(4096)
            except OSError:
                data = b""
            if data:
                c.answer += data
            else:
                c.closed_at = time.monotonic() - t0


def run(target, count, hold, trickle):
    host, port = hostport.split(target, 80)
    hosthdr = target.strip()
    conns = [Conn(i + 1) for i in range(count)]
    t0 = time.monotonic()
    for c in conns:
        try:
            c.sock = socket.create_connection((host, port), timeout=3)
            c.sock.sendall(("GET /api/state HTTP/1.1\r\nHost: %s\r\n" % hosthdr).encode("ascii"))
            c.sock.setblocking(False)
        except OSError as e:
            c.error = str(e)
            c.sock = None
    if all(c.sock is None for c in conns):
        print("error: no connection to %s:%d (%s)" % (host, port, conns[0].error), file=sys.stderr)
        return 2
    accepted = sum(1 for c in conns if c.sock)
    print("%d of %d connections accepted, holding the requests for %.0f s" % (accepted, count, hold))
    next_byte = t0 + trickle
    while time.monotonic() - t0 < hold:
        poll(conns, t0, 0.2)
        if time.monotonic() >= next_byte:
            next_byte += trickle
            for c in conns:
                if c.sock and c.closed_at is None:
                    try:
                        c.sock.send(b"X")  # part of a header line that never ends early
                    except OSError:
                        c.closed_at = time.monotonic() - t0
    # Complete the requests that are still open.
    for c in conns:
        if c.sock and c.closed_at is None:
            try:
                c.sock.send(b"-Slow: 1\r\n\r\n")
            except OSError:
                c.closed_at = time.monotonic() - t0
    end = time.monotonic() + 5
    while time.monotonic() < end and any(c.sock and c.closed_at is None and not c.answer for c in conns):
        poll(conns, t0, 0.2)
    poll(conns, t0, 0.2)

    open_without_answer = 0
    for c in conns:
        if not c.sock:
            what = "refused (%s)" % c.error
        elif c.answer:
            what = "answer %s%s" % (c.status(), "" if c.closed_at is None else ", closed at %.1f s" % c.closed_at)
        elif c.closed_at is not None:
            what = "closed by the server at %.1f s without an answer" % c.closed_at
        else:
            what = "still open without an answer"
            open_without_answer += 1
        print("  %2d  %s" % (c.n, what))
        if c.sock:
            c.sock.close()
    statuses = {}
    for c in conns:
        if c.answer:
            statuses[c.status()] = statuses.get(c.status(), 0) + 1
    closed = sum(1 for c in conns if c.sock and c.closed_at is not None and not c.answer)
    print("summary: %d accepted, %d refused, %d closed without answer, answers %s, %d still open" % (
        accepted, count - accepted, closed, statuses or "{}", open_without_answer))
    return 1 if open_without_answer else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("host", help="host[:port] of your own device, e.g. 192.168.1.50 or 127.0.0.1:8099")
    ap.add_argument("-n", "--connections", type=int, default=30, help="number of connections (default 30)")
    ap.add_argument("--hold", type=float, default=10.0, help="seconds before the requests are completed (default 10)")
    ap.add_argument("--trickle", type=float, default=2.0, help="seconds between the slow header bytes (default 2)")
    args = ap.parse_args()
    if not 1 <= args.connections <= 100:
        ap.error("--connections must be 1..100")
    try:
        return run(args.host, args.connections, args.hold, args.trickle)
    except ValueError as e:
        print("error: %s" % e, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
