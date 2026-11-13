#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""LXI / VXI-11 example for the CYD HF generator (standard library only).

Finds generators the way `lxi discover` does (a portmapper GETPORT broadcast for
the VXI-11 core program to UDP port 111) and talks to them over VXI-11:

  1. discovery: every device that answers is asked *IDN? over VXI-11
  2. with --address: *IDN?, the CH1 frequency and the error queue, or the
     commands given with --command (queries are the ones ending in '?')
  3. with --pyvisa: the same through PyVISA (resource TCPIP::<ip>::INSTR)

It never switches an output on unless you send such a command yourself.

Usage:
    python lxi_example.py --discover
    python lxi_example.py --discover --broadcast 192.168.1.255
    python lxi_example.py --address 192.168.1.50
    python lxi_example.py --address cyd-hf-1a2b.local --command "FREQ 7.1MHZ" --command "FREQ?"
    python lxi_example.py --address 192.168.1.50 --pyvisa    # pip install pyvisa pyvisa-py

Requirements: Python 3.8+; "pip install pyvisa pyvisa-py" only for --pyvisa.
"""
import argparse
import os
import socket
import struct
import sys
import time

PMAP_PROG, PMAP_VERS, PMAP_PORT = 100000, 2, 111
PMAPPROC_GETPORT = 3
CORE_PROG, CORE_VERS = 0x0607AF, 1
IPPROTO_TCP = 6
CREATE_LINK, DEVICE_WRITE, DEVICE_READ, DEVICE_READSTB, DESTROY_LINK = 10, 11, 12, 13, 23
FLAG_END = 0x08
REASON_END = 0x04
VXI11_ERRORS = {
    1: "syntax error", 3: "device not accessible", 4: "invalid link identifier",
    5: "parameter error", 6: "channel not established", 8: "operation not supported",
    9: "out of resources", 11: "device locked by another link", 12: "no lock held by this link",
    15: "I/O timeout", 17: "I/O error", 21: "invalid address", 23: "abort",
    29: "channel already established",
}


class RpcError(Exception):
    pass


class Vxi11Error(Exception):
    def __init__(self, code):
        super().__init__("VXI-11 error %d (%s)" % (code, VXI11_ERRORS.get(code, "unknown")))
        self.code = code


def opaque(data):
    return struct.pack(">I", len(data)) + data + b"\0" * (-len(data) % 4)


def call_message(xid, prog, vers, proc, args=b""):
    # CALL, RPC version 2, AUTH_NONE credentials and verifier.
    return struct.pack(">IIIIIIIIII", xid, 0, 2, prog, vers, proc, 0, 0, 0, 0) + args


def parse_reply(msg, xid):
    """The results of an accepted, successful reply; raises RpcError otherwise."""
    if len(msg) < 12:
        raise RpcError("short reply")
    rxid, mtype, stat = struct.unpack(">III", msg[:12])
    if rxid != xid or mtype != 1:
        raise RpcError("not the reply to this call")
    if stat != 0:
        raise RpcError("call denied (reject_stat %d)" % struct.unpack(">I", msg[12:16])[0])
    _flavor, vlen = struct.unpack(">II", msg[12:20])
    at = 20 + vlen + (-vlen % 4)
    (accept,) = struct.unpack(">I", msg[at:at + 4])
    if accept != 0:
        raise RpcError("call not accepted (accept_stat %d)" % accept)
    return msg[at + 4:]


class RpcTcpClient:
    """ONC RPC over TCP with record marking."""

    def __init__(self, host, port, timeout):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.xid = int.from_bytes(os.urandom(4), "big")

    def call(self, prog, vers, proc, args=b""):
        self.xid = (self.xid + 1) & 0xFFFFFFFF
        msg = call_message(self.xid, prog, vers, proc, args)
        self.sock.sendall(struct.pack(">I", 0x80000000 | len(msg)) + msg)
        return parse_reply(self._record(), self.xid)

    def _record(self):
        data = b""
        while True:
            (head,) = struct.unpack(">I", self._exact(4))
            data += self._exact(head & 0x7FFFFFFF)
            if head & 0x80000000:
                return data

    def _exact(self, n):
        buf = b""
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise RpcError("connection closed")
            buf += chunk
        return buf

    def close(self):
        self.sock.close()


def getport(host, timeout, prog=CORE_PROG, vers=CORE_VERS):
    """The TCP port of a program from the portmapper on TCP 111."""
    pm = RpcTcpClient(host, PMAP_PORT, timeout)
    try:
        res = pm.call(PMAP_PROG, PMAP_VERS, PMAPPROC_GETPORT, struct.pack(">IIII", prog, vers, IPPROTO_TCP, 0))
    finally:
        pm.close()
    (port,) = struct.unpack(">I", res[:4])
    if port == 0:
        raise RpcError("VXI-11 core program not registered")
    return port


class Vxi11Instrument:
    """A VXI-11 link to device inst0 (create_link, device_write, device_read, destroy_link)."""

    def __init__(self, host, port=None, timeout=3.0):
        self.timeout_ms = int(timeout * 1000)
        self.rpc = RpcTcpClient(host, port or getport(host, timeout), timeout)
        res = self.rpc.call(CORE_PROG, CORE_VERS, CREATE_LINK, struct.pack(">iII", 1, 0, 0) + opaque(b"inst0"))
        error, self.lid, _abort_port, self.max_recv = struct.unpack(">IIII", res[:16])
        if error:
            self.rpc.close()
            raise Vxi11Error(error)

    def write(self, text):
        data = (text + "\n").encode("ascii")
        for i in range(0, len(data), self.max_recv):
            chunk = data[i:i + self.max_recv]
            flags = FLAG_END if i + self.max_recv >= len(data) else 0
            args = struct.pack(">IIII", self.lid, self.timeout_ms, 0, flags) + opaque(chunk)
            error, _size = struct.unpack(">II", self.rpc.call(CORE_PROG, CORE_VERS, DEVICE_WRITE, args)[:8])
            if error:
                raise Vxi11Error(error)

    def read(self):
        out = b""
        while True:
            args = struct.pack(">IIIIII", self.lid, 65536, self.timeout_ms, 0, 0, 0)
            res = self.rpc.call(CORE_PROG, CORE_VERS, DEVICE_READ, args)
            error, reason, n = struct.unpack(">III", res[:12])
            if error:
                raise Vxi11Error(error)
            out += res[12:12 + n]
            if reason & REASON_END:
                return out.decode("ascii", "replace").rstrip("\n")

    def query(self, text):
        self.write(text)
        return self.read()

    def status_byte(self):
        args = struct.pack(">IIII", self.lid, 0, 0, self.timeout_ms)
        error, stb = struct.unpack(">II", self.rpc.call(CORE_PROG, CORE_VERS, DEVICE_READSTB, args)[:8])
        if error:
            raise Vxi11Error(error)
        return stb

    def close(self):
        try:
            self.rpc.call(CORE_PROG, CORE_VERS, DESTROY_LINK, struct.pack(">I", self.lid))
        finally:
            self.rpc.close()


def discover(broadcast, timeout):
    """(ip, core port) of every device that answers the GETPORT broadcast."""
    xid = int.from_bytes(os.urandom(4), "big")
    msg = call_message(xid, PMAP_PROG, PMAP_VERS, PMAPPROC_GETPORT,
                       struct.pack(">IIII", CORE_PROG, CORE_VERS, IPPROTO_TCP, 0))
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.settimeout(0.2)
    s.sendto(msg, (broadcast, PMAP_PORT))
    found = {}
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        try:
            data, (ip, _port) = s.recvfrom(1024)
        except socket.timeout:
            continue
        try:
            (port,) = struct.unpack(">I", parse_reply(data, xid)[:4])
        except (RpcError, struct.error):
            continue
        if port:
            found[ip] = port
    s.close()
    return sorted(found.items())


def run_vxi11(host, commands, timeout):
    inst = Vxi11Instrument(host, timeout=timeout)
    try:
        for cmd in commands:
            if cmd.rstrip().endswith("?"):
                print("%-12s -> %s" % (cmd, inst.query(cmd)))
            else:
                inst.write(cmd)
                print("%-12s (sent)" % cmd)
        print("status byte  -> %d" % inst.status_byte())
    finally:
        inst.close()


def run_pyvisa(host, commands, timeout):
    import pyvisa  # optional

    rm = pyvisa.ResourceManager("@py")
    # open_timeout: pyvisa-py defaults to about 0.1 s for the connect; WiFi power save needs more.
    inst = rm.open_resource("TCPIP::%s::INSTR" % host, timeout=int(timeout * 1000),
                            open_timeout=int(timeout * 1000))
    inst.read_termination = "\n"
    inst.write_termination = "\n"
    try:
        for cmd in commands:
            if cmd.rstrip().endswith("?"):
                print("%-12s -> %s" % (cmd, inst.query(cmd)))
            else:
                inst.write(cmd)
                print("%-12s (sent)" % cmd)
    finally:
        inst.close()
        rm.close()


def main():
    ap = argparse.ArgumentParser(description="LXI / VXI-11 example for the CYD HF generator.")
    ap.add_argument("--discover", action="store_true", help="find devices by VXI-11 broadcast")
    ap.add_argument("--broadcast", default="255.255.255.255", help="broadcast address for --discover")
    ap.add_argument("--address", help="device address (IP or name.local)")
    ap.add_argument("--command", action="append", help="command to send (repeatable); default *IDN?, FREQ?, SYST:ERR?")
    ap.add_argument("--pyvisa", action="store_true", help="use PyVISA (pyvisa-py) instead of the built-in client")
    ap.add_argument("--timeout", type=float, default=3.0, help="seconds (default 3)")
    a = ap.parse_args()
    if not a.discover and not a.address:
        ap.error("give --discover or --address")
    commands = a.command or ["*IDN?", "FREQ?", "SYST:ERR?"]
    try:
        if a.discover:
            devices = discover(a.broadcast, a.timeout)
            if not devices:
                print("no VXI-11 device answered")
                return 1
            for ip, port in devices:
                try:
                    inst = Vxi11Instrument(ip, port, a.timeout)
                    try:
                        idn = inst.query("*IDN?")
                    finally:
                        inst.close()
                except (OSError, RpcError, Vxi11Error) as e:
                    idn = "(no *IDN?: %s)" % e
                print("%-15s TCPIP::%s::INSTR  %s" % (ip, ip, idn))
        if a.address:
            (run_pyvisa if a.pyvisa else run_vxi11)(a.address, commands, a.timeout)
    except (OSError, RpcError, Vxi11Error) as e:
        print("error: %s" % e, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
