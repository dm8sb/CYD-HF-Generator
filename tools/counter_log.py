#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Frequency counter logger for MESSPLAN C7, C8 (AF39): reads a SCPI instrument in the rhythm of
its gate time and writes "time;frequency;temperature" lines (CSV with ';', ISO time in UTC).

The counter is reached over TCP (raw SCPI socket, e.g. port 5025) or a serial port (needs
pyserial); the query is configurable because every counter names it differently (default
"READ?"). A second query on the same instrument (--temp-query) fills the temperature column,
otherwise it stays empty. Try it without hardware against tools/fake_scpi_device.py:

    python tools/counter_log.py --tcp 127.0.0.1:5025 --query "FREQ:ACT?" --interval 1 --count 5
    python tools/counter_log.py --usb COM9 --query "MEAS:FREQ?" --interval 10 -o c7.csv
"""
import argparse
import datetime
import socket
import sys
import time

import hostport


class TcpLink:
    def __init__(self, target, timeout):
        host, port = hostport.split(target, 5025)
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.buf = b""

    def query(self, cmd):
        self.sock.sendall(cmd.encode("ascii") + b"\n")
        while b"\n" not in self.buf:
            data = self.sock.recv(4096)
            if not data:
                raise OSError("connection closed")
            self.buf += data
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode("ascii", "replace").strip()

    def close(self):
        self.sock.close()


class SerialLink:
    def __init__(self, port, baud, timeout):
        import serial  # pyserial, only for --usb
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = port, baud, timeout
        self.s.dtr = False
        self.s.rts = False
        self.s.open()

    def query(self, cmd):
        self.s.write(cmd.encode("ascii") + b"\n")
        line = self.s.readline()
        if not line:
            raise OSError("no answer to %s" % cmd)
        return line.decode("ascii", "replace").strip()

    def close(self):
        self.s.close()


def number(text):
    """The first field of an answer as a float (counters answer e.g. "+1.0000000012E+07")."""
    return float(text.split(",")[0])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    grp = ap.add_mutually_exclusive_group(required=True)
    grp.add_argument("--tcp", metavar="HOST[:PORT]", help="raw SCPI socket, port defaults to 5025")
    grp.add_argument("--usb", metavar="PORT", help="serial port (pyserial), DTR/RTS stay off")
    ap.add_argument("--baud", type=int, default=115200, help="serial speed (default 115200)")
    ap.add_argument("--query", default="READ?", help="frequency query of the counter (default READ?)")
    ap.add_argument("--temp-query", help="query for the temperature column (empty without)")
    ap.add_argument("--interval", type=float, default=10.0, help="seconds between readings = gate time (default 10)")
    ap.add_argument("--count", type=int, default=0, help="number of readings (default 0 = until Ctrl-C)")
    ap.add_argument("--timeout", type=float, default=30.0, help="answer timeout in seconds (default 30)")
    ap.add_argument("-o", "--output", help="CSV file (appended; default stdout)")
    args = ap.parse_args()
    try:
        link = TcpLink(args.tcp, args.timeout) if args.tcp else SerialLink(args.usb, args.baud, args.timeout)
    except (OSError, ValueError, ImportError) as e:
        print("cannot connect: %s" % e, file=sys.stderr)
        return 2
    out = open(args.output, "a", encoding="utf-8", newline="") if args.output else sys.stdout
    if not args.output or out.tell() == 0:
        out.write("time;frequency;temperature\n")
    n = 0
    next_t = time.monotonic()
    try:
        while not args.count or n < args.count:
            stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%fZ")
            freq = number(link.query(args.query))
            temp = link.query(args.temp_query) if args.temp_query else ""
            out.write("%s;%.6f;%s\n" % (stamp, freq, temp))
            out.flush()
            n += 1
            next_t += args.interval
            time.sleep(max(0.0, next_t - time.monotonic()))
    except KeyboardInterrupt:
        pass
    except (OSError, ValueError) as e:
        print("error after %d readings: %s" % (n, e), file=sys.stderr)
        return 1
    finally:
        link.close()
        if args.output:
            out.close()
    print("%d readings" % n, file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
