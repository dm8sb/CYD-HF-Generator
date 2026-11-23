#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Fake SCPI device for testing the host tools offline (not the real firmware).

A small TCP server that implements a subset of USER_MANUAL_EN.md section 15:
IDN, OPC, ESR/STB/TST, OUTPut, FREQuency, CH3 IQ/PHASe, DRIVe, OFFState, SSC,
TUNing, CORRection, STEP, SI5351, HEAP, LOG, ERRor, VERSion, SAVE, line limits,
compound lines, the rate limit and *RST. Not implemented: BACKlight, IDENtify,
*ESE/*SRE, status register semantics.

    python fake_scpi_device.py --port 5025
    python device_check.py --tcp 127.0.0.1:5025

With --vxi11 it also serves VXI-11 on the same fake device: the portmapper on
UDP/TCP --portmap-port (default 111) and the core channel on TCP 9009, a reduced
model of the firmware (create_link inst0, device_write/read with END and one
answer message per read, error 15 when nothing waits, readstb, clear,
lock/unlock, remote/local, destroy_link; trigger, SRQ, docmd and the interrupt
channel give error 8). Not modelled: fragment limits beyond 2 KB, idle timeout,
one connection only.

    python fake_scpi_device.py --vxi11
    python lxi_example.py --address 127.0.0.1

It prints "SAVE RECEIVED" if a client sends SYST:SAVE.
"""
import argparse
import re
import socketserver
import struct
import threading
import time

import device_constants as dc

FREQ_MIN, FREQ_MAX = dc.FREQ_MIN, dc.FREQ_MAX  # include/constants.h (AF48)


def kw_match(tok, spec):
    """tok matches SPEC like 'FREQuency' in short (capitals) or long form."""
    tok = tok.upper()
    return tok == spec.upper() or tok == "".join(c for c in spec if c.isupper())


class Device:
    def __init__(self):
        self.lock = threading.Lock()
        self.freq = dict(dc.DEFAULT_FREQ)
        self.out = {1: 0, 2: 0, 3: 0}
        self.step, self.corr, self.drive = dc.DEFAULT_STEP, 0.0, dc.DEFAULT_DRIVE
        self.tuning, self.ssc, self.off = "FRAC", "OFF", "LOW"
        self.master, self.phase = 1, 90
        self.errors = []
        self.log = ["boot line 1", "boot line 2"]
        self.stamps = []
        self.saved = False

    def err(self, code, text):
        if len(self.errors) < 16:
            self.errors.append('%d,"%s"' % (code, text))

    # -- numbers -----------------------------------------------------------
    def number(self, p, lo, hi, default):
        """Return a float or None (error already queued)."""
        u = p.upper()
        if p.startswith('"'):
            self.err(-104, "Data type error")
            return None
        if u == "MIN":
            return lo
        if u == "MAX":
            return hi
        if u == "DEF":
            return default
        # Like the firmware's lexer: anything after number and suffix ("7MHZ 8MHZ", "0x10")
        # is an invalid character, not an illegal value.
        rest = re.match(r"^[+-]?\d+\.?\d*(?:E[+-]?\d+)?\s*[A-Z]*(.*)$", u)
        if rest and rest.group(1).strip():
            self.err(-101, "Invalid character")
            return None
        m = re.match(r"^([+-]?\d+\.?\d*(?:E[+-]?\d+)?)\s*(HZ|KHZ|MHZ)?$", u)
        if not m:
            self.err(-224, "Illegal parameter value")
            return None
        v = float(m.group(1)) * {None: 1, "HZ": 1, "KHZ": 1e3, "MHZ": 1e6}[m.group(2)]
        if v < lo or v > hi:
            self.err(-222, "Data out of range")
            return None
        return v

    # -- one command -------------------------------------------------------
    # Constant answers (a set form is accepted silently, as before).
    FIXED = {
        "*IDN": "CYD,HF-Generator,AA:BB:CC:DD:EE:FF,2.1.0",
        "*ESR": "0", "*STB": "0", "*TST": "0",
        "SYST:VERS": "1999.0",
        "SYST:SI5351": "CONN",
        "SYST:HEAP": "150000,120000,60000",
        "OUTP:REM": "ALLOWED",
        "SOUR3:IQ:RANG": "%d,%d" % (dc.IQ_MIN, dc.IQ_MAX),
    }
    NO_PARAM = ("*CLS", "*OPC", "*RST", "SYST:SAVE")  # set forms without a parameter
    ENUMS = ("SYST:TUN", "SSC", "OUTP:OFFS", "SOUR3:IQ:MAST", "SOUR3:PHAS")

    def execute(self, segs, ch, params):
        """segs: canonical-matched list of names; returns answer or None."""
        path = ":".join(segs)
        query = path.endswith("?")
        path = path.rstrip("?")
        if len(params) > 1:
            self.err(-108, "Parameter not allowed")
            return None
        if not query and not params and path not in self.NO_PARAM:
            self.err(-109, "Missing parameter")
            return None
        p = params[0] if params else None
        if path in self.FIXED:
            return self.FIXED[path]
        handler = self.HANDLERS.get(path)
        if handler:
            return handler(self, query, p, ch)
        if path in self.ENUMS:
            return self.enum_setting(path, query, p)
        self.err(-113, "Undefined header")
        return None

    def _opc(self, query, p, ch):
        return "1" if query else None

    def _rst(self, query, p, ch):
        """*RST: all outputs off, defaults in RAM, the correction kept, nothing saved."""
        if query:
            self.err(-113, "Undefined header")
            return None
        self.freq = dict(dc.DEFAULT_FREQ)
        self.out = {1: 0, 2: 0, 3: 0}
        self.step, self.drive = dc.DEFAULT_STEP, dc.DEFAULT_DRIVE
        self.tuning, self.ssc, self.off = "FRAC", "OFF", "LOW"
        self.master, self.phase = 1, 90
        self.log.append("[APP] *RST: defaults in RAM, not saved")
        return None

    def _cls(self, query, p, ch):
        self.errors.clear()

    def _log(self, query, p, ch):
        return '""' if not self.log else '"%s"' % self.log.pop(0)

    def _err_count(self, query, p, ch):
        return str(len(self.errors))

    def _err_next(self, query, p, ch):
        return self.errors.pop(0) if self.errors else '0,"No error"'

    def _save(self, query, p, ch):
        self.saved = True
        print("SAVE RECEIVED")

    def _output(self, query, p, ch):
        if query:
            return str(self.out[ch])
        v = p.upper()
        if v not in ("ON", "OFF", "1", "0"):
            self.err(-224, "Illegal parameter value")
        else:
            self.out[ch] = 1 if v in ("ON", "1") else 0

    def _condition(self, query, p, ch):
        return "ON" if self.out[ch] else "OFF"

    def _freq(self, query, p, ch):
        if query:
            return str(self.freq.get(ch, self.freq[1]))
        v = self.number(p, FREQ_MIN, FREQ_MAX, dc.DEFAULT_FREQ[1])
        if v is not None:
            self.freq[ch] = int(round(v))

    def _actual(self, query, p, ch):
        return "%.1f" % self.freq.get(ch, self.freq[1])

    def _step(self, query, p, ch):
        if query:
            return str(self.step)
        v = self.number(p, dc.STEPS[0], dc.STEPS[-1], dc.DEFAULT_STEP)
        if v is None:
            return
        if v in dc.STEPS:
            self.step = int(v)
        else:
            self.err(-224, "Illegal parameter value")

    def _corr(self, query, p, ch):
        if query:
            return "%.2f" % self.corr
        v = self.number(p, -dc.CORRECTION_MAX / 100, dc.CORRECTION_MAX / 100, 0)
        if v is not None:
            self.corr = round(v, 2)

    def _drive(self, query, p, ch):
        if query:
            return str(self.drive)
        if p in tuple(str(d) for d in dc.DRIVES):
            self.drive = int(p)
        else:
            self.err(-224, "Illegal parameter value")

    HANDLERS = {
        "*OPC": _opc, "*CLS": _cls, "*RST": _rst,
        "SYST:LOG": _log, "SYST:ERR:COUN": _err_count, "SYST:ERR": _err_next, "SYST:ERR:NEXT": _err_next,
        "SYST:SAVE": _save,
        "OUTP": _output, "OUTP:STAT": _output, "OUTP:COND": _condition,
        "FREQ": _freq, "FREQ:CW": _freq, "FREQ:ACT": _actual,
        "SYST:STEP": _step, "SYST:CORR": _corr, "OUTP:DRIV": _drive,
    }

    def enum_setting(self, path, query, p):
        attr, short, choices = {
            "SYST:TUN": ("tuning", {"FRACTIONAL": "FRAC", "LNOISE": "LNO", "INTEGER": "INT"}, None),
            "SSC": ("ssc", None, ["OFF", "DOWN05", "DOWN15", "CENT05"]),
            "OUTP:OFFS": ("off", None, ["LOW", "HIGH", "HIZ"]),
            "SOUR3:IQ:MAST": ("master", None, ["1", "2"]),
            "SOUR3:PHAS": ("phase", None, ["0", "90", "180", "270"]),
        }[path]
        if query:
            return str(getattr(self, attr))
        v = p.upper()
        if short:
            v = short.get(v, v)
            choices = list(short.values())
        if v not in choices:
            self.err(-224, "Illegal parameter value")
        elif attr in ("master", "phase"):
            setattr(self, attr, int(v))
        else:
            setattr(self, attr, v)
        return None

    # -- one line ----------------------------------------------------------
    def handle_line(self, raw):
        """raw bytes without LF; returns the answer line or None."""
        with self.lock:
            if len(raw) > 256:
                self.err(-363, "Input buffer overrun")
                return None
            if any((b < 32 and b != 9) or b > 126 for b in raw):
                self.err(-101, "Invalid character")
                return None
            cmds = raw.decode("ascii").split(";")
            if len(cmds) > 16:
                self.err(-223, "Too many data elements")
                return None
            answers = []
            prev = []
            for c in cmds:
                c = c.strip()
                if not c:
                    continue
                ans, prev = self.one(c, prev)
                if ans is not None:
                    answers.append(ans)
            return ";".join(answers) if answers else None

    def one(self, cmd, prev):
        m = re.match(r"^(\S+)\s*(.*)$", cmd)
        head, rest = m.group(1), m.group(2)
        # Parameters are separated by commas; a space inside one is left to number().
        params = [x.strip() for x in rest.split(",")] if rest else []
        is_query = head.endswith("?")
        if head.startswith("*"):
            name = head.rstrip("?").upper()
            return self.rate_and_run([name + ("?" if is_query else "")], 1, params, is_query), prev
        root = head.startswith(":")
        toks = head.lstrip(":").rstrip("?").split(":")
        if not root and prev:
            toks = prev + toks
        # canonicalize
        grammar = {
            None: ["SOURce", "OUTPut", "SYSTem", "FREQuency", "SSC"],
            "SOURce": ["FREQuency", "SSC", "IQ", "PHASe"],
            "OUTPut": ["STATe", "CONDition", "DRIVe", "OFFState", "REMote"],
            "SYSTem": ["TUNing", "CORRection", "STEP", "SI5351", "HEAP", "LOG", "SAVE", "ERRor", "VERSion"],
            "FREQuency": ["CW", "ACTual"],
            "ERRor": ["NEXT", "COUNt"],
            "IQ": ["MASTer", "RANGe"],
        }
        segs, ch, parent = [], 1, None
        for i, t in enumerate(toks):
            name, suffix = t, ""
            hit = next((s for s in grammar.get(parent, []) if kw_match(name, s)), None)
            if hit is None:
                mm = re.match(r"^(.*?[A-Za-z])(\d+)$", t)
                if mm:
                    name, suffix = mm.group(1), mm.group(2)
                    hit = next((s for s in grammar.get(parent, []) if kw_match(name, s)), None)
            if hit is None:
                self.err(-113, "Undefined header")
                return None, []
            if suffix:
                if hit not in ("SOURce", "OUTPut") or not 1 <= int(suffix) <= 3:
                    self.err(-114, "Header suffix out of range")
                    return None, []
                ch = int(suffix)
            segs.append(hit)
            parent = hit
        # SOURce:FREQ -> FREQ, drop the SOURce prefix; map to short canonical names
        if segs and segs[0] == "SOURce" and len(segs) > 1:
            segs = segs[1:]
        canon = ":".join(s if s in ("SYSTem", "FREQuency") else s for s in segs)
        canon = (canon.replace("SYSTem", "SYST").replace("FREQuency", "FREQ")
                 .replace("OUTPut", "OUTP").replace("SOURce", "SOUR").replace("STATe", "STAT")
                 .replace("CONDition", "COND").replace("DRIVe", "DRIV").replace("OFFState", "OFFS")
                 .replace("REMote", "REM").replace("TUNing", "TUN").replace("CORRection", "CORR")
                 .replace("ERRor", "ERR").replace("VERSion", "VERS").replace("COUNt", "COUN")
                 .replace("ACTual", "ACT").replace("MASTer", "MAST").replace("PHASe", "PHAS")
                 .replace("RANGe", "RANG").replace("NEXT", "NEXT"))
        if canon.startswith("IQ") or canon == "PHAS":
            canon = "SOUR3:" + canon
        if canon.startswith("SOUR3:") is False and ch == 3 and canon.startswith("FREQ") and not is_query:
            self.err(-221, "Settings conflict;CH3 follows its master")
            return None, segs[:-1]
        if canon.startswith("SOUR3:") and ch != 3 and segs[0] != "SOURce":
            ch = 3
        full = canon + ("?" if is_query else "")
        return self.rate_and_run([full], ch, params, is_query), segs[:-1]

    def rate_and_run(self, segs, ch, params, is_query):
        if not is_query:
            now = time.monotonic()
            self.stamps = [t for t in self.stamps if now - t < 1.0]
            if len(self.stamps) >= 20:
                self.err(-200, "Execution error;Too many commands")
                return None
            self.stamps.append(now)
        return self.execute(segs, ch, params)


class Handler(socketserver.StreamRequestHandler):
    def handle(self):
        dev = self.server.dev
        while True:
            raw = self.rfile.readline(4096)
            if not raw:
                return
            line = raw.rstrip(b"\n").rstrip(b"\r")
            ans = dev.handle_line(line)
            if ans is not None:
                self.wfile.write(ans.encode("ascii") + b"\n")
                self.wfile.flush()


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


def serve(port=0):
    srv = Server(("127.0.0.1", port), Handler)
    srv.dev = Device()
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


# -- VXI-11 (reduced model of src/remote/vxi11.cpp) ------------------------------------------
VXI11_CORE_PORT = 9009


def rpc_reply(xid, accept, results=b""):
    """Accepted reply with an AUTH_NONE verifier."""
    return struct.pack(">IIIIII", xid, 1, 0, 0, 0, accept) + results


def rpc_call(msg):
    """(xid, prog, vers, proc, args) of a call with AUTH_NONE/AUTH_SYS credentials, or None."""
    try:
        xid, mtype, rpcvers, prog, vers, proc = struct.unpack(">IIIIII", msg[:24])
        at = 24
        for _ in range(2):  # credentials, verifier
            (n,) = struct.unpack(">I", msg[at + 4:at + 8])
            at += 8 + n + (-n % 4)
    except struct.error:
        return None
    if mtype != 0 or rpcvers != 2 or at > len(msg):
        return None
    return xid, prog, vers, proc, msg[at:]


def portmap_answer(msg):
    call = rpc_call(msg)
    if not call:
        return None
    xid, prog, vers, proc, args = call
    if prog != 100000:
        return rpc_reply(xid, 1)
    if vers != 2:
        return rpc_reply(xid, 2, struct.pack(">II", 2, 2))
    if proc == 0:
        return rpc_reply(xid, 0)
    if proc == 3 and len(args) >= 16:
        p, v, prot, _ = struct.unpack(">IIII", args[:16])
        port = VXI11_CORE_PORT if (p, v, prot) == (0x0607AF, 1, 6) else 0
        return rpc_reply(xid, 0, struct.pack(">I", port))
    return rpc_reply(xid, 3)


class CoreLink:
    """The VXI-11 core channel of one TCP connection (one link)."""

    def __init__(self, dev):
        self.dev, self.lid, self.next_lid, self.locked = dev, 0, 1, False
        self.partial, self.answer, self.in_message = b"", b"", False

    def answer_call(self, msg):
        call = rpc_call(msg)
        if not call:
            return None
        xid, prog, vers, proc, a = call
        if prog != 0x0607AF:
            return rpc_reply(xid, 1)
        if vers != 1:
            return rpc_reply(xid, 2, struct.pack(">II", 1, 1))

        def u32(i):
            return struct.unpack(">I", a[4 * i:4 * i + 4])[0]

        def ok(results):
            return rpc_reply(xid, 0, results)

        try:
            if proc == 10:  # create_link
                n = u32(3)
                name = a[16:16 + n].decode("ascii", "replace").lower()
                err = 21 if name != "inst0" else 9 if self.lid else 0
                if not err:
                    self.lid, self.next_lid = self.next_lid, self.next_lid + 1
                    self.locked, self.answer, self.partial = bool(u32(1)), b"", b""
                return ok(struct.pack(">IIII", err, self.lid if not err else 0, 0, 1024))
            linked = self.lid and u32(0) == self.lid
            if proc == 11:  # device_write
                n = u32(4)
                data, end = a[20:20 + n], bool(u32(3) & 8)
                if not linked:
                    return ok(struct.pack(">II", 4, 0))
                if not self.in_message and self.answer:
                    self.answer = b""
                    self.dev.err(-410, "Query INTERRUPTED")
                lines = (self.partial + data).split(b"\n")
                self.partial = lines.pop()
                if end and self.partial:
                    lines.append(self.partial)
                    self.partial = b""
                for line in lines:
                    ans = self.dev.handle_line(line.rstrip(b"\r"))
                    if ans is not None:
                        self.answer += ans.encode("ascii") + b"\n"
                self.in_message = not end
                return ok(struct.pack(">II", 0, n))
            if proc == 12:  # device_read
                if not linked or not self.answer:
                    return ok(struct.pack(">III", 4 if not linked else 15, 0, 0))
                end = self.answer.index(b"\n") + 1
                n = min(end, u32(1), 1024)
                data, self.answer = self.answer[:n], self.answer[n:]
                reason = (4 if n == end else 0) | (1 if n == u32(1) else 0)
                return ok(struct.pack(">III", 0, reason, n) + data + b"\0" * (-n % 4))
            if proc in (13, 14, 15, 16, 17, 18, 19, 23):
                if not linked:
                    return ok(struct.pack(">II", 4, 0) if proc == 13 else struct.pack(">I", 4))
                if proc == 13:
                    return ok(struct.pack(">II", 0, 0x10 if self.answer else 0))
                err = 0
                if proc == 14:
                    err = 8
                elif proc == 15:
                    self.answer, self.partial, self.in_message = b"", b"", False
                elif proc == 18:
                    self.locked = True
                elif proc == 19:
                    err, self.locked = (0, False) if self.locked else (12, False)
                elif proc == 23:
                    self.lid, self.locked = 0, False
                return ok(struct.pack(">I", err))
            if proc in (20, 25, 26):
                return ok(struct.pack(">I", 8))
            if proc == 22:
                return ok(struct.pack(">II", 8, 0))
            return rpc_reply(xid, 0 if proc == 0 else 3)
        except (struct.error, IndexError):
            return rpc_reply(xid, 4)


class RpcStreamHandler(socketserver.BaseRequestHandler):
    """Record marking over TCP; the server's program gives the reply of each record."""

    def handle(self):
        program = self.server.program_factory(self.server.dev)
        sock, buf, record = self.request, b"", b""
        while True:
            while len(buf) < 4:
                chunk = sock.recv(4096)
                if not chunk:
                    return
                buf += chunk
            (head,) = struct.unpack(">I", buf[:4])
            n = head & 0x7FFFFFFF
            if len(record) + n > 2048:
                return  # like the firmware: a record that is too large closes
            while len(buf) < 4 + n:
                chunk = sock.recv(4096)
                if not chunk:
                    return
                buf += chunk
            record += buf[4:4 + n]
            buf = buf[4 + n:]
            if head & 0x80000000:
                reply = program(record)
                record = b""
                if reply:
                    sock.sendall(struct.pack(">I", 0x80000000 | len(reply)) + reply)


class PortmapUdpHandler(socketserver.BaseRequestHandler):
    def handle(self):
        data, sock = self.request
        reply = portmap_answer(data)
        if reply and len(reply) <= len(data):
            sock.sendto(reply, self.client_address)


class UdpServer(socketserver.ThreadingUDPServer):
    daemon_threads = True


def serve_vxi11(dev, portmap_port=111, host="127.0.0.1"):
    """Portmapper (UDP and TCP) and core channel on the fake device dev."""
    servers = []
    for port, factory in ((portmap_port, lambda d: portmap_answer),
                          (VXI11_CORE_PORT, lambda d: CoreLink(d).answer_call)):
        srv = Server((host, port), RpcStreamHandler)
        srv.dev, srv.program_factory = dev, factory
        servers.append(srv)
    servers.append(UdpServer((host, portmap_port), PortmapUdpHandler))
    for srv in servers:
        threading.Thread(target=srv.serve_forever, daemon=True).start()
    return servers


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Fake SCPI device for offline tests of the host tools.")
    ap.add_argument("--port", type=int, default=5025)
    ap.add_argument("--vxi11", action="store_true", help="also serve VXI-11 (portmapper and core channel 9009)")
    ap.add_argument("--portmap-port", type=int, default=111, help="portmapper port with --vxi11 (default 111)")
    a = ap.parse_args()
    s = serve(a.port)
    print("fake SCPI device on 127.0.0.1:%d (Ctrl-C to stop)" % s.server_address[1])
    if a.vxi11:
        serve_vxi11(s.dev, a.portmap_port)
        print("VXI-11: portmapper on 127.0.0.1:%d (UDP/TCP), core channel on 127.0.0.1:%d"
              % (a.portmap_port, VXI11_CORE_PORT))
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
