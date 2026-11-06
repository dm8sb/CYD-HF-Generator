#!/usr/bin/env python3
"""Automated SCPI check for the CYD HF generator (smoke test / MESSPLAN L6, L-S).

Talks to a real device over USB serial (--usb) or TCP (--tcp). It only changes
RAM state: the settings it touches are read first and written back at the end,
all outputs are switched off, and SYST:SAVE is never sent. *RST is sent only
with --allow-rst. Outputs are switched ON only with --outputs (the generator
outputs RF: no unfiltered antenna!).

Checks: *IDN? format, empty error queue, valid settings round trips, rejected
inputs with the documented error codes (state unchanged), compound lines, line
limits, rate limit, *IDN? timing, SYST:LOG? drain.

Examples:
    python device_check.py --usb COM5
    python device_check.py --tcp cyd-hf-1a2b.local --repeat 1000 --json > report.json
    python device_check.py --tcp 127.0.0.1:5025 --outputs

Exit code: 0 = all checks passed (SKIPs allowed), 1 = at least one FAIL,
2 = no connection / no answer to *IDN?.
"""
import argparse
import json
import re
import socket
import statistics
import sys
import time

# ---------------------------------------------------------------------------
# Expected values (USER_MANUAL_EN.md section 15). Edit here when the firmware
# or the manual changes.
# ---------------------------------------------------------------------------
EXPECT = {
    "idn_regex": r"^CYD,HF-Generator,[^,]+,\d+\.\d+\.\d+",
    "scpi_version": "1999.0",
    "freq_min": 8000,              # Hz
    "freq_max": 160000000,         # Hz
    "freq_tol_hz": 0.5,            # tolerance when comparing FREQ? answers
    "freq_act_tol_hz": 2.0,        # FREQ:ACT? vs. request with correction 0
    "no_error": '0,"No error"',
    "error_queue_len": 16,
    "log_buffer_len": 32,
    "max_line": 256,
    "max_commands": 16,
    "max_setting_cmds_per_s": 20,
    "rate_error": '-200,"Execution error;Too many commands"',
    "idn_median_ms": 50.0,         # MESSPLAN L6
    "iq_min_hz": 4730000,          # CH3 IQ range minimum without spread
}

# (label, set command, query, expected answer, kind)
# kind: int = whole number, float = decimal (tol 0.005), text = case-insensitive,
# range = "<min>,<max>" numbers. expected None = only check that it is in range.
ROUNDTRIPS = [
    ("CH1 freq 7.1 MHZ", "SOUR1:FREQ 7.1 MHZ", "SOUR1:FREQ?", 7100000, "int"),
    ("CH1 freq 455khz lower case", "freq 455khz", "FREQ?", 455000, "int"),
    ("CH1 freq 7100000 plain Hz", "FREQ 7100000", "FREQ?", 7100000, "int"),
    ("CH1 freq 7.1E6 exponent", "FREQuency:CW 7.1E6", "FREQ:CW?", 7100000, "int"),
    ("CH1 freq 10 MHz no space", "FREQ 10MHZ", "FREQ?", 10000000, "int"),
    ("CH1 freq rounds to 1 Hz", "FREQ 7100000.4", "FREQ?", 7100000, "int"),
    ("CH1 freq MIN", "FREQ MIN", "FREQ?", EXPECT["freq_min"], "int"),
    ("CH1 freq MAX", "FREQ MAX", "FREQ?", EXPECT["freq_max"], "int"),
    ("CH1 freq DEF in range", "FREQ DEF", "FREQ?", None, "int"),
    ("CH2 freq 3.5 MHZ", "SOUR2:FREQ 3.5MHZ", "SOUR2:FREQ?", 3500000, "int"),
    ("CH2 freq 14.2 MHZ", "SOUR2:FREQ:CW 14.2 MHZ", "SOUR2:FREQ?", 14200000, "int"),
    ("step 100", "SYST:STEP 100", "SYST:STEP?", 100, "int"),
    ("step 1 MHz", "SYST:STEP 1000000", "SYST:STEP?", 1000000, "int"),
    ("step 1", "SYST:STEP 1", "SYST:STEP?", 1, "int"),
    ("correction 12.34", "SYST:CORR 12.34", "SYST:CORR?", 12.34, "float"),
    ("correction -50", "SYST:CORR -50", "SYST:CORR?", -50.0, "float"),
    ("correction MAX", "SYST:CORR MAX", "SYST:CORR?", 50.0, "float"),
    ("correction MIN", "SYST:CORR MIN", "SYST:CORR?", -50.0, "float"),
    ("correction DEF", "SYST:CORR DEF", "SYST:CORR?", 0.0, "float"),
    ("drive 2", "OUTP:DRIV 2", "OUTP:DRIV?", 2, "int"),
    ("drive 6", "OUTP:DRIVe 6", "OUTP:DRIV?", 6, "int"),
    ("drive 8", "OUTP:DRIV 8", "OUTP:DRIV?", 8, "int"),
    ("drive 4", "OUTP:DRIV 4", "OUTP:DRIV?", 4, "int"),
    ("tuning FRAC", "SYST:TUN FRAC", "SYST:TUN?", "FRAC", "text"),
    ("tuning LNO", "SYST:TUN LNO", "SYST:TUN?", "LNO", "text"),
    ("tuning long form LNOise", "SYSTem:TUNing LNOise", "SYST:TUN?", "LNO", "text"),
    ("tuning INT", "SYST:TUN INT", "SYST:TUN?", "INT", "text"),
    ("tuning back to FRAC", "SYST:TUN FRACtional", "SYST:TUN?", "FRAC", "text"),
    ("spread DOWN05", "SSC DOWN05", "SSC?", "DOWN05", "text"),
    ("spread DOWN15", "SOUR1:SSC DOWN15", "SSC?", "DOWN15", "text"),
    ("spread CENT05", "SSC CENT05", "SSC?", "CENT05", "text"),
    ("spread OFF", "SSC OFF", "SSC?", "OFF", "text"),
    ("off state HIZ", "OUTP:OFFS HIZ", "OUTP:OFFS?", "HIZ", "text"),
    ("off state LOW", "OUTP:OFFState LOW", "OUTP:OFFS?", "LOW", "text"),
    ("CH3 master 2", "SOUR3:IQ:MAST 2", "SOUR3:IQ:MAST?", 2, "int"),
    ("CH3 master 1", "SOUR3:IQ:MASTer 1", "SOUR3:IQ:MAST?", 1, "int"),
    ("CH3 phase 90", "SOUR3:PHAS 90", "SOUR3:PHAS?", 90, "int"),
    ("CH3 phase 180", "SOUR3:PHAS 180", "SOUR3:PHAS?", 180, "int"),
    ("CH3 phase 270", "SOUR3:PHAS 270", "SOUR3:PHAS?", 270, "int"),
    ("CH3 phase 0", "SOUR3:PHAS 0", "SOUR3:PHAS?", 0, "int"),
]
# Only with --outputs (a switched-off output then carries DC for a moment).
ROUNDTRIPS_OUTPUTS_ONLY = [
    ("off state HIGH", "OUTP:OFFS HIGH", "OUTP:OFFS?", "HIGH", "text"),
]

# (command, expected error code, probe query whose answer must not change)
REJECTS = [
    ("FREQ NAN", -224, "FREQ?"),
    ("FREQ INF", -224, "FREQ?"),
    ("FREQ 0x10", -101, "FREQ?"),
    ("FREQ 7.1 FOO", -224, "FREQ?"),
    ("FREQ 200MHZ", -222, "FREQ?"),
    ("FREQ 1000", -222, "FREQ?"),
    ("FREQ 1E400", -222, "FREQ?"),
    ('FREQ "abc"', -104, "FREQ?"),
    ("FREQ", -109, "FREQ?"),
    ("FREQ 7MHZ 8MHZ", -101, "FREQ?"),
    ("FREQ 7MHZ,8MHZ", -108, "FREQ?"),
    ("FOO:BAR 1", -113, "FREQ?"),
    ("OUTP4 ON", -114, "OUTP1?"),
    ("SOUR5:FREQ 7MHZ", -114, "FREQ?"),
    ("OUTP1 MAYBE", -224, "OUTP1?"),
    ("OUTP:DRIV 5", -224, "OUTP:DRIV?"),
    ("SYST:CORR 51", -222, "SYST:CORR?"),
    ("SYST:STEP 5", -224, "SYST:STEP?"),
    ("SYST:TUN FOO", -224, "SYST:TUN?"),
    ("SOUR3:PHAS 45", -224, "SOUR3:PHAS?"),
    ("SOUR3:IQ:MAST 3", -224, "SOUR3:IQ:MAST?"),
]

# Never sent: SYST:SAVE.  Only with --allow-rst: *RST.


# ---------------------------------------------------------------------------
# Transports
# ---------------------------------------------------------------------------
class LinkTimeout(Exception):
    pass


class UsbLink:
    def __init__(self, port, timeout):
        import serial

        ser = serial.Serial()
        ser.port = port
        ser.baudrate = 115200
        ser.timeout = timeout
        ser.dtr = False   # before open(): otherwise the board restarts
        ser.rts = False
        ser.open()
        self.ser = ser
        self.sync()

    def sync(self):
        """Start the SCPI session and skip log lines that were still on the wire."""
        self.ser.write(b"\n")
        time.sleep(0.3)
        self.ser.reset_input_buffer()

    def write_raw(self, data):
        self.ser.write(data)

    def readline(self):
        raw = self.ser.readline()
        if not raw.endswith(b"\n"):
            raise LinkTimeout("no answer")
        return raw.decode("ascii", "replace").strip()

    def close(self):
        self.ser.close()


class TcpLink:
    def __init__(self, target, timeout):
        host, _, port = target.rpartition(":") if ":" in target else (target, "", "")
        self.sock = socket.create_connection((host, int(port or 5025)), timeout=timeout)
        self.buf = b""

    def write_raw(self, data):
        self.sock.sendall(data)

    def readline(self):
        while b"\n" not in self.buf:
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                raise LinkTimeout("no answer")
            if not data:
                raise ConnectionError("connection closed by the device")
            self.buf += data
        line, _, self.buf = self.buf.partition(b"\n")
        return line.decode("ascii", "replace").strip()

    def close(self):
        self.sock.close()


class Dev:
    """SCPI helper: queries, paced setting commands, error queue access."""

    def __init__(self, link, pace):
        self.link = link
        self.pace = pace
        self._last_set = 0.0

    def write_line(self, text):
        self.link.write_raw((text + "\n").encode("ascii"))

    def q(self, cmd):
        self.write_line(cmd)
        return self.link.readline()

    def set(self, cmd):
        """Setting command, paced to stay below the device's rate limit."""
        wait = self._last_set + self.pace - time.monotonic()
        if wait > 0:
            time.sleep(wait)
        self.write_line(cmd)
        self._last_set = time.monotonic()

    def resync(self):
        """After a timeout: ask *OPC? until its answer '1' arrives, dropping late answers."""
        for _ in range(6):
            try:
                self.write_line("*OPC?")
                while self.link.readline() != "1":
                    pass
                return
            except LinkTimeout:
                continue

    def err(self):
        return self.q("SYST:ERR?")

    def drain(self, limit=64):
        """Read SYST:ERR? until the queue is empty; return the entries."""
        out = []
        for _ in range(limit):
            e = self.err()
            if e.startswith("0,"):
                return out
            out.append(e)
        out.append("<queue did not empty>")
        return out


def code_of(err):
    try:
        return int(err.split(",", 1)[0])
    except ValueError:
        return None


# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------
class Report:
    def __init__(self, quiet):
        self.items = []
        self.quiet = quiet  # --json: human lines go to stderr
        self.on_timeout = None  # called after a timeout to resynchronize the stream

    def add(self, name, status, detail=""):
        self.items.append({"name": name, "status": status, "detail": detail})
        out = sys.stderr if self.quiet else sys.stdout
        out.write("%-4s %s%s\n" % (status, name, "  [%s]" % detail if detail else ""))
        out.flush()

    def count(self, status):
        return sum(1 for i in self.items if i["status"] == status)


def run(rep, name, fn):
    """Run one check. fn returns (ok, detail), or (None, reason) for SKIP."""
    try:
        ok, detail = fn()
        rep.add(name, "SKIP" if ok is None else ("PASS" if ok else "FAIL"), detail)
    except LinkTimeout as e:
        rep.add(name, "FAIL", "timeout: %s" % e)
        if rep.on_timeout:
            rep.on_timeout()
    except Exception as e:  # keep going: one broken check must not stop the rest
        rep.add(name, "FAIL", "%s: %s" % (type(e).__name__, e))


def num(text):
    return float(text.strip().strip('"'))


def matches(answer, expected, kind):
    if kind == "text":
        return answer.strip().upper() == str(expected).upper()
    value = num(answer)
    if expected is None:
        return EXPECT["freq_min"] <= value <= EXPECT["freq_max"]
    tol = 0.005 if kind == "float" else EXPECT["freq_tol_hz"]
    return abs(value - expected) <= tol


# ---------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------
# Queries for everything the checks may change; restored at the end.
STATE_QUERIES = [
    ("freq1", "SOUR1:FREQ?", "SOUR1:FREQ"),
    ("freq2", "SOUR2:FREQ?", "SOUR2:FREQ"),
    ("step", "SYST:STEP?", "SYST:STEP"),
    ("corr", "SYST:CORR?", "SYST:CORR"),
    ("drive", "OUTP:DRIV?", "OUTP:DRIV"),
    ("tuning", "SYST:TUN?", "SYST:TUN"),
    ("ssc", "SSC?", "SSC"),
    ("offstate", "OUTP:OFFS?", "OUTP:OFFS"),
    ("master", "SOUR3:IQ:MAST?", "SOUR3:IQ:MAST"),
    ("phase", "SOUR3:PHAS?", "SOUR3:PHAS"),
]


def read_state(dev):
    state = {}
    for key, query, _ in STATE_QUERIES:
        try:
            state[key] = dev.q(query)
        except LinkTimeout:
            state[key] = None
    return state


def restore_state(dev, orig):
    """Outputs off, then every touched setting back to its original value."""
    for n in (1, 2, 3):
        dev.set("OUTP%d OFF" % n)
    tuning = orig.get("tuning")
    dev.set("SYST:TUN FRAC")            # spread is locked in Integer
    for key, _, cmd in STATE_QUERIES:
        if key in ("tuning",) or orig.get(key) is None:
            continue
        dev.set("%s %s" % (cmd, orig[key]))
    if tuning:
        dev.set("SYST:TUN %s" % tuning)
    time.sleep(0.2)


def check_identity(dev):
    def f():
        a = dev.q("*IDN?")
        return re.match(EXPECT["idn_regex"], a) is not None, a
    return f


def check_error_drain(dev):
    def f():
        errs = dev.drain()
        ok = "<queue did not empty>" not in errs
        return ok, "drained %d entr%s%s" % (len(errs), "y" if len(errs) == 1 else "ies",
                                            ": " + "; ".join(errs[:3]) if errs else "")
    return f


def check_basic_queries(dev):
    items = [
        ("*OPC?", lambda a: a == "1"),
        ("SYST:VERS?", lambda a: a == EXPECT["scpi_version"]),
        ("*TST?", lambda a: a in ("0", "1")),
        ("*ESR?", lambda a: a.lstrip("+-").isdigit()),
        ("*STB?", lambda a: a.lstrip("+-").isdigit()),
        ("SYST:SI5351?", lambda a: a in ("CONN", "NFO", "BLOC", "UNKN")),
        ("OUTP:REM?", lambda a: a in ("ALLOWED", "LOCAL")),
        ("SYST:ERR:COUN?", lambda a: a.isdigit()),
        ("OUTP1:COND?", lambda a: a in ("ON", "OFF", "ERR", "WAIT", "NOHW")),
        ("OUTP3:COND?", lambda a: a in ("ON", "OFF", "ERR", "WAIT", "NOHW")),
        ("OUTP1?", lambda a: a in ("0", "1")),
        ("FREQ:ACT?", lambda a: num(a) >= 0),
        ("SOUR3:FREQ?", lambda a: num(a) >= 0),
    ]
    out = []
    for cmd, ok_fn in items:
        def f(cmd=cmd, ok_fn=ok_fn):
            a = dev.q(cmd)
            try:
                return bool(ok_fn(a)), a
            except ValueError:
                return False, a
        out.append(("query %s" % cmd, f))
    def heap():
        a = dev.q("SYST:HEAP?")
        parts = a.split(",")
        return len(parts) == 3 and all(p.strip().isdigit() for p in parts), a
    def rng():
        a = dev.q("SOUR3:IQ:RANG?")
        p = [num(x) for x in a.split(",")]
        return len(p) == 2 and p[0] < p[1] and p[0] >= EXPECT["iq_min_hz"] - 1, a
    out.append(("query SYST:HEAP?", heap))
    out.append(("query SOUR3:IQ:RANG?", rng))
    return out


def roundtrip_check(dev, label, setcmd, query, expected, kind):
    def f():
        dev.set(setcmd)
        a = dev.q(query)
        errs = dev.drain()
        ok = matches(a, expected, kind) and not errs
        detail = "%s -> %s" % (setcmd, a)
        if errs:
            detail += " errors: " + "; ".join(errs)
        return ok, detail
    return f


def reject_check(dev, cmd, code, probe):
    def f():
        dev.drain()
        before = dev.q(probe)
        dev.set(cmd)
        e = dev.err()
        after = dev.q(probe)
        extra = dev.drain()
        ok = code_of(e) == code and before == after and not extra
        detail = "%s -> %s" % (cmd, e)
        if before != after:
            detail += "; state changed %s -> %s" % (before, after)
        if extra:
            detail += "; extra errors: " + "; ".join(extra)
        return ok, detail
    return f


def check_conflict(dev):
    def f():
        if dev.q("OUTP:REM?") != "LOCAL":
            return None, "OUTP:REM? is not LOCAL (ON would not be refused); not testable"
        dev.drain()
        dev.set("OUTP1 ON")
        e = dev.err()
        state = dev.q("OUTP1?")
        dev.drain()
        return code_of(e) == -221 and state == "0", "OUTP1 ON -> %s, OUTP1? %s" % (e, state)
    return f


def check_compound(dev):
    def f1():
        dev.drain()
        dev.set("SOUR1:FREQ 7.1MHZ;:SOUR2:FREQ 3.5MHZ")
        a = dev.q("SOUR1:FREQ?;:SOUR2:FREQ?")
        errs = dev.drain()
        return a.replace(" ", "") == "7100000;3500000" and not errs, a
    def f2():
        n = dev.q("OUTP1?")
        a = dev.q("FREQ?;:OUTP1?")
        return a == "7100000;%s" % n, a
    def f3():
        a = dev.q("*OPC?;*OPC?;SYST:VERS?")
        return a == "1;1;%s" % EXPECT["scpi_version"], a
    def f4():
        a = dev.q("SYST:STEP 1000;:SYST:STEP?")  # the setting answers nothing, the query one value
        return a == "1000", a
    return [("compound ';:' two settings, two queries", f1),
            ("compound query line 'FREQ?;:OUTP1?'", f2),
            ("compound ';' relative queries", f3),
            ("compound set then query on one line", f4)]


def check_line_limits(dev):
    def maxlen_ok():
        dev.drain()
        line = "SYST:VERS?" + " " * (EXPECT["max_line"] - len("SYST:VERS?"))
        assert len(line) == EXPECT["max_line"]
        a = dev.q(line)
        return a == EXPECT["scpi_version"], "256 chars -> %s" % a

    def too_long():
        dev.drain()
        line = "SYST:VERS?" + " " * (EXPECT["max_line"] + 1 - len("SYST:VERS?"))
        dev.write_line(line)                 # discarded: no answer expected
        e = dev.err()
        return code_of(e) == -363, "257 chars -> %s" % e

    def many():
        dev.drain()
        n = EXPECT["max_commands"]
        ok16 = dev.q(";".join(["*OPC?"] * n))
        good = ok16 == ";".join(["1"] * n)
        dev.write_line(";".join(["SYST:VERS?"] * (n + 1)))
        e = dev.err()
        return good and code_of(e) == -223, "16 cmds -> ok=%s; 17 cmds -> %s" % (good, e)

    def nul():
        dev.drain()
        dev.link.write_raw(b"SYST:VERS?\x00\n")
        e = dev.err()
        return code_of(e) == -101, "NUL byte -> %s" % e

    def nonascii():
        dev.drain()
        dev.link.write_raw(b"SYST:VERS? \xc3\xa4\n")
        e = dev.err()
        return code_of(e) == -101, "non-ASCII byte -> %s" % e

    def alive():
        a = dev.q("*OPC?")
        dev.drain()
        return a == "1", "still answers after the broken lines"

    return [("line of exactly 256 chars accepted", maxlen_ok),
            ("line of 257 bytes -> -363", too_long),
            ("16 commands accepted, 17 commands -> -223", many),
            ("NUL byte -> -101", nul),
            ("non-ASCII byte -> -101", nonascii),
            ("device still answers", alive)]


def check_rate_limit(dev, orig_step):
    def f():
        dev.drain()
        time.sleep(1.2)                      # empty the one-second window
        n = EXPECT["max_setting_cmds_per_s"] + 10
        # one burst, written as a single block: harmless "set step to its current value"
        dev.link.write_raw(("SYST:STEP %s\n" % orig_step * n).encode("ascii"))
        time.sleep(0.5)
        errs = dev.drain()
        limited = [e for e in errs if code_of(e) == -200 and "Too many commands" in e]
        time.sleep(1.2)
        dev.set("SYST:STEP %s" % orig_step)  # allowed again after the window
        after = dev.drain()
        ok = len(limited) >= 1 and not after
        detail = "%d commands in a burst -> %d x %s" % (n, len(limited),
                                                       EXPECT["rate_error"] if limited else "no -200")
        if after:
            detail += "; still limited after 1.2 s: " + "; ".join(after)
        return ok, detail
    return f


def check_timing(dev, repeat):
    def f():
        times = []
        bad = 0
        for _ in range(repeat):
            t0 = time.perf_counter()
            a = dev.q("*IDN?")
            times.append((time.perf_counter() - t0) * 1000.0)
            if not re.match(EXPECT["idn_regex"], a):
                bad += 1
        times.sort()
        med = statistics.median(times)
        p99 = times[min(len(times) - 1, int(len(times) * 0.99))]
        detail = "%d x *IDN?: median %.1f ms, p99 %.1f ms, max %.1f ms, invalid answers %d" % (
            repeat, med, p99, times[-1], bad)
        return bad == 0 and med < EXPECT["idn_median_ms"], detail
    return f


def check_log_drain(dev):
    def f():
        lines = 0
        for _ in range(EXPECT["log_buffer_len"] + 8):
            a = dev.q("SYST:LOG?")
            if a == '""':
                return lines <= EXPECT["log_buffer_len"], "%d buffered line(s), then \"\"" % lines
            lines += 1
        return False, "SYST:LOG? never answered \"\""
    return f


def check_outputs(dev):
    """Only with --outputs: switch CH1 and CH3 on briefly at 7.1 MHz."""
    def ch1():
        dev.set("SYST:CORR 0")
        dev.set("SOUR1:FREQ 7.1MHZ")
        dev.set("OUTP1 ON")
        time.sleep(0.5)
        st, cond, act = dev.q("OUTP1?"), dev.q("OUTP1:COND?"), dev.q("FREQ:ACT?")
        dev.set("OUTP1 OFF")
        off = dev.q("OUTP1?")
        errs = dev.drain()
        if cond == "NOHW":
            return None, "no SI5351 connected (NOHW)"
        ok = (st == "1" and cond == "ON" and off == "0" and not errs
              and abs(num(act) - 7100000) <= EXPECT["freq_act_tol_hz"])
        return ok, "OUTP1? %s, COND %s, ACT %s, after OFF %s%s" % (
            st, cond, act, off, " errors: " + "; ".join(errs) if errs else "")

    def ch3():
        if dev.q("OUTP1:COND?") == "NOHW":
            return None, "no SI5351 connected (NOHW)"
        dev.set("SOUR1:FREQ 7.1MHZ")
        dev.set("SOUR3:IQ:MAST 1")
        dev.set("SOUR3:PHAS 90")
        dev.set("OUTP1 ON")
        dev.set("OUTP3 ON")
        time.sleep(0.5)
        cond, f3 = dev.q("OUTP3:COND?"), dev.q("SOUR3:FREQ?")
        dev.set("OUTP3 OFF")
        dev.set("OUTP1 OFF")
        errs = dev.drain()
        ok = cond == "ON" and abs(num(f3) - 7100000) <= 1 and not errs
        return ok, "CH3 COND %s, freq %s%s" % (cond, f3, " errors: " + "; ".join(errs) if errs else "")

    return [("outputs: CH1 on/off at 7.1 MHz", ch1),
            ("outputs: CH3 IQ follows CH1 (90 deg)", ch3)]


def check_rst(dev):
    def f():
        dev.drain()
        dev.set("*RST")
        time.sleep(0.5)
        states = [dev.q("OUTP%d?" % n) for n in (1, 2, 3)]
        errs = dev.drain()
        return states == ["0", "0", "0"] and not errs, "outputs after *RST: %s" % ",".join(states)
    return f


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description="Automated SCPI check of the CYD HF generator. Changes only RAM state and "
                    "restores it; never sends SYST:SAVE.",
        epilog="SAFETY: the generator outputs RF. Outputs stay off unless --outputs is given; "
               "never connect an antenna unfiltered.")
    grp = ap.add_mutually_exclusive_group(required=True)
    grp.add_argument("--usb", metavar="PORT", help="serial port, e.g. COM5 (DTR/RTS stay off)")
    grp.add_argument("--tcp", metavar="HOST[:PORT]", help="TCP address, port defaults to 5025")
    ap.add_argument("--repeat", type=int, default=100, metavar="N",
                    help="number of *IDN? queries for the timing check (default 100; use 1000 for L6)")
    ap.add_argument("--outputs", action="store_true",
                    help="also switch outputs ON briefly (RF!); without it no output is enabled")
    ap.add_argument("--allow-rst", action="store_true", help="also test *RST (all settings in RAM reset)")
    ap.add_argument("--json", action="store_true", help="machine-readable report on stdout")
    ap.add_argument("--timeout", type=float, default=2.0, help="answer timeout in seconds (default 2)")
    ap.add_argument("--pace", type=float, default=0.06,
                    help="minimum seconds between setting commands (default 0.06, limit is 20/s)")
    args = ap.parse_args()

    rep = Report(quiet=args.json)
    try:
        link = UsbLink(args.usb, args.timeout) if args.usb else TcpLink(args.tcp, args.timeout)
    except Exception as e:
        sys.stderr.write("cannot connect: %s\n" % e)
        return 2
    dev = Dev(link, args.pace)
    rep.on_timeout = dev.resync

    device_id = None
    code = 0
    try:
        try:
            device_id = dev.q("*IDN?")
        except LinkTimeout:
            sys.stderr.write("no answer to *IDN?\n")
            return 2

        run(rep, "*IDN? format", check_identity(dev))
        run(rep, "SYST:ERR? drains to 0,\"No error\"", check_error_drain(dev))
        run(rep, "SYST:ERR? is empty after the drain", lambda: (
            dev.err() == EXPECT["no_error"], "answer 0,\"No error\""))
        for name, fn in check_basic_queries(dev):
            run(rep, name, fn)

        orig = read_state(dev)
        outs = [dev.q("OUTP%d?" % n) for n in (1, 2, 3)]
        rep.add("settings read for restore", "PASS",
                ", ".join("%s=%s" % kv for kv in sorted(orig.items())) + "; outputs=" + ",".join(outs))

        try:
            table = list(ROUNDTRIPS) + (ROUNDTRIPS_OUTPUTS_ONLY if args.outputs else [])
            for label, setcmd, query, expected, kind in table:
                run(rep, "roundtrip " + label, roundtrip_check(dev, label, setcmd, query, expected, kind))
            for cmd, ecode, probe in REJECTS:
                run(rep, "reject %r -> %d" % (cmd, ecode), reject_check(dev, cmd, ecode, probe))
            run(rep, "reject OUTP1 ON with Remote outputs local only -> -221", check_conflict(dev))
            for name, fn in check_compound(dev):
                run(rep, name, fn)
            for name, fn in check_line_limits(dev):
                run(rep, name, fn)
            if args.outputs:
                for name, fn in check_outputs(dev):
                    run(rep, name, fn)
            if args.allow_rst:
                run(rep, "*RST switches outputs off", check_rst(dev))
            else:
                rep.add("*RST", "SKIP", "needs --allow-rst")
            run(rep, "rate limit: > 20 setting commands in 1 s -> -200", check_rate_limit(
                dev, orig.get("step") or "1000"))
        finally:
            # Restore even after a crash or Ctrl-C.
            def restore():
                restore_state(dev, orig)
                bad = []
                for key, query, _ in STATE_QUERIES:
                    if orig.get(key) is not None:
                        now = dev.q(query)
                        same = now == orig[key]
                        if not same:
                            try:
                                same = abs(num(now) - num(orig[key])) < 0.006
                            except ValueError:
                                pass
                        if not same:
                            bad.append("%s %s != %s" % (key, now, orig[key]))
                now_out = [dev.q("OUTP%d?" % n) for n in (1, 2, 3)]
                if now_out != ["0", "0", "0"]:
                    bad.append("outputs %s" % ",".join(now_out))
                dev.drain()
                return not bad, "; ".join(bad) or "all settings restored, outputs off"
            run(rep, "restore settings, outputs off", restore)

        run(rep, "SYST:LOG? drains to \"\"", check_log_drain(dev))
        run(rep, "*IDN? timing", check_timing(dev, args.repeat))
    except KeyboardInterrupt:
        sys.stderr.write("interrupted\n")
        code = 1
    finally:
        link.close()

    summary = {"pass": rep.count("PASS"), "fail": rep.count("FAIL"), "skip": rep.count("SKIP")}
    if summary["fail"]:
        code = 1
    line = "Summary: %d PASS, %d FAIL, %d SKIP" % (summary["pass"], summary["fail"], summary["skip"])
    if args.json:
        sys.stderr.write(line + "\n")
        json.dump({"device": device_id, "outputs_tested": args.outputs, "summary": summary,
                   "checks": rep.items}, sys.stdout, indent=2)
        sys.stdout.write("\n")
    else:
        print(line)
    return code


if __name__ == "__main__":
    sys.exit(main())
