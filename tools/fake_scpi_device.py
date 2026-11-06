#!/usr/bin/env python3
"""Fake SCPI device for testing the host tools offline (not the real firmware).

A small TCP server that implements a subset of USER_MANUAL_EN.md section 15:
IDN, OPC, ESR/STB/TST, OUTPut, FREQuency, CH3 IQ/PHASe, DRIVe, OFFState, SSC,
TUNing, CORRection, STEP, SI5351, HEAP, LOG, ERRor, VERSion, SAVE, line limits,
compound lines and the rate limit. Not implemented: *RST, BACKlight, IDENtify,
*ESE/*SRE, status register semantics.

    python fake_scpi_device.py --port 5025
    python device_check.py --tcp 127.0.0.1:5025

It prints "SAVE RECEIVED" if a client sends SYST:SAVE.
"""
import argparse
import re
import socketserver
import threading
import time

FREQ_MIN, FREQ_MAX = 8000, 160000000


def kw_match(tok, spec):
    """tok matches SPEC like 'FREQuency' in short (capitals) or long form."""
    tok = tok.upper()
    return tok == spec.upper() or tok == "".join(c for c in spec if c.isupper())


class Device:
    def __init__(self):
        self.lock = threading.Lock()
        self.freq = {1: 10000000, 2: 14000000}
        self.out = {1: 0, 2: 0, 3: 0}
        self.step, self.corr, self.drive = 1000, 0.0, 4
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
    def execute(self, segs, ch, params):
        """segs: canonical-matched list of names; returns answer or None."""
        path = ":".join(segs)
        query = path.endswith("?")
        path = path.rstrip("?")
        p = params[0] if len(params) == 1 else None
        if len(params) > 1 or (not query and not params and path not in ("*CLS", "*OPC", "SYST:SAVE")):
            self.err(-108 if len(params) > 1 else -109,
                     "Parameter not allowed" if len(params) > 1 else "Missing parameter")
            return None
        if path == "*IDN":
            return "CYD,HF-Generator,AA:BB:CC:DD:EE:FF,1.4.0"
        if path == "*OPC":
            return "1" if query else None
        if path == "*CLS":
            self.errors.clear()
            return None
        if path in ("*ESR", "*STB"):
            return "0"
        if path == "*TST":
            return "0"
        if path == "SYST:VERS":
            return "1999.0"
        if path == "SYST:SI5351":
            return "CONN"
        if path == "SYST:HEAP":
            return "150000,120000,60000"
        if path == "SYST:LOG":
            return '""' if not self.log else '"%s"' % self.log.pop(0)
        if path == "SYST:ERR:COUN":
            return str(len(self.errors))
        if path in ("SYST:ERR", "SYST:ERR:NEXT"):
            return self.errors.pop(0) if self.errors else '0,"No error"'
        if path == "SYST:SAVE":
            self.saved = True
            print("SAVE RECEIVED")
            return None
        if path == "OUTP:REM":
            return "ALLOWED"
        if path in ("OUTP", "OUTP:STAT"):
            if query:
                return str(self.out[ch])
            v = p.upper()
            if v not in ("ON", "OFF", "1", "0"):
                self.err(-224, "Illegal parameter value")
            else:
                self.out[ch] = 1 if v in ("ON", "1") else 0
            return None
        if path == "OUTP:COND":
            return "ON" if self.out[ch] else "OFF"
        if path in ("FREQ", "FREQ:CW"):
            if query:
                return str(self.freq.get(ch, self.freq[1]))
            v = self.number(p, FREQ_MIN, FREQ_MAX, 10000000)
            if v is not None:
                self.freq[ch] = int(round(v))
            return None
        if path == "FREQ:ACT":
            return "%.1f" % self.freq.get(ch, self.freq[1])
        if path == "SYST:STEP":
            if query:
                return str(self.step)
            v = self.number(p, 1, 1000000, 1000)
            if v is not None:
                if v in (1, 10, 100, 1000, 10000, 100000, 1000000):
                    self.step = int(v)
                else:
                    self.err(-224, "Illegal parameter value")
            return None
        if path == "SYST:CORR":
            if query:
                return "%.2f" % self.corr
            v = self.number(p, -50, 50, 0)
            if v is not None:
                self.corr = round(v, 2)
            return None
        if path == "OUTP:DRIV":
            if query:
                return str(self.drive)
            if p in ("2", "4", "6", "8"):
                self.drive = int(p)
            else:
                self.err(-224, "Illegal parameter value")
            return None
        if path in ("SYST:TUN", "SSC", "OUTP:OFFS", "SOUR3:IQ:MAST", "SOUR3:PHAS"):
            return self.enum_setting(path, query, p)
        if path == "SOUR3:IQ:RANG":
            return "4730000,160000000"
        self.err(-113, "Undefined header")
        return None

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


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="Fake SCPI device for offline tests of the host tools.")
    ap.add_argument("--port", type=int, default=5025)
    a = ap.parse_args()
    s = serve(a.port)
    print("fake SCPI device on 127.0.0.1:%d (Ctrl-C to stop)" % s.server_address[1])
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
