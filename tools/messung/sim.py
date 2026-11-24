# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Simulated bench for dry runs and tests: generator, SDS, SSA and SDG answer the same SCPI text
as the real instruments (the subset the scripts use), from one physical model:

- crystal error of the generator (+9.7 ppm) and the correction it is told
- oscilloscope time base off by +18 ppm, channel skew, 8-bit quantisation, noise, 200 MHz
  bandwidth; the GPS reference is exact
- square waves with drive-dependent level, 49.6 % duty cycle, a CLK2 skew of 150 ps
- spectra with harmonics, tuning-mode spurs, analyzer noise floor and phase noise
- dropouts when the firmware would interrupt an output (IQ realignment, spread, Low noise
  divider change)

It runs on a virtual clock: waiting costs no time. Nothing here claims how the real device
behaves; it only makes every code path of the scripts run.
"""
import io
import math
import re

import numpy as np

XTAL_PPM = 9.7
SDS_TB_PPM = 18.0
CLK2_SKEW_S = 30e-12
CH_SKEW_S = {1: 0.0, 2: 40e-12, 3: 60e-12, 4: -30e-12}
DUTY = 0.496
VPP_50 = {2: 0.45, 4: 0.85, 6: 1.2, 8: 1.5}
P1_DBM = {2: -0.5, 4: 4.6, 6: 7.7, 8: 9.5}
SIM_POINTS = 1000000

PLUGS_G = {
    "scope": {1: "CLK1", 2: None, 3: "CLK2", 4: "GPS"},
    "term": {1: 50, 2: 1e6, 3: 50, 4: 1e6},
    "ssa": "CLK0",
    "pad_db": 20.0,
}


def _phi(x):
    return 0.5 * (1.0 + np.tanh(0.7978845608 * (x + 0.044715 * x ** 3)))


def _bmp(w=160, h=96, shade=60):
    from PIL import Image

    img = Image.new("RGB", (w, h), (shade, shade, shade + 20))
    out = io.BytesIO()
    img.save(out, format="BMP")
    return out.getvalue()


def _block(payload):
    n = str(len(payload))
    return b"#" + str(len(n)).encode() + n.encode() + payload


class SimLink:
    """Text link onto one simulated instrument."""

    def __init__(self, bench, handler, name):
        self.bench = bench
        self.handler = handler
        self.name = "sim:" + name
        self._pending = None

    def write(self, cmd):
        self.bench.now += 0.001  # every command takes time: events and re-arming never coincide
        for part in cmd.split(";"):
            part = part.strip()
            if part:
                self.handler(part, query=False)

    def query(self, cmd):
        self.bench.now += 0.001
        return str(self.handler(cmd.strip(), query=True))

    def query_block(self, cmd):
        """Payload of the definite-length block, as links.VisaLink returns it."""
        from links import parse_block

        ans = self.handler(cmd.strip(), query=True)
        if isinstance(ans, (bytes, bytearray)):
            return parse_block(bytes(ans))
        raise IOError("no block for " + cmd)

    def close(self):
        pass

    def pulse_reset(self):
        self.bench.dut_reset()


# =========================================================================== generator


class SimDut:
    def __init__(self, bench):
        self.bench = bench
        self.errors = []
        self.corr = 0.0
        self.reset()
        self.wlan = False
        self.faultiq = False

    def reset(self):
        self.freq = {1: 10e6, 2: 14e6}
        self.out = {1: False, 2: False, 3: False}
        self.master = 1
        self.phase = 90
        self.drive = 4
        self.tuning = "FRAC"
        self.ssc = "OFF"
        self.offstate = "LOW"
        self.step = 1000
        self.backlight = 80
        self.coupled = False
        self.offset = 0.0

    # model ------------------------------------------------------------

    def actual(self, ch):
        if ch == 3:
            ch = self.master
        f = self.freq[ch]
        if self.tuning == "INT":
            return self._integer(f)
        if (900e6 / f).is_integer():
            f_act = float(f)  # integer divider of the 900 MHz VCO (10 MHz = 900/90): exact
        else:
            frac = ((f * 7919) % 997) / 997.0 - 0.5  # deterministic rounding error, <= 0.25 Hz
            f_act = f + 0.5 * frac
        if ch == 1 and self.ssc != "OFF":
            f_act *= {"DOWN05": 1 - 0.0025, "DOWN15": 1 - 0.0075, "CENT05": 1.0}[self.ssc]
        return round(f_act, 1)

    @staticmethod
    def _integer(f):
        best = None
        for n in range(24, 37):
            vco = 25e6 * n
            d = max(4, int(round(vco / f / 2.0)) * 2)
            for dd in (d - 2, d, d + 2):
                if dd < 4:
                    continue
                fo = vco / dd
                if best is None or abs(fo - f) < abs(best - f):
                    best = fo
        return round(best, 1)

    def real(self, src):
        """Physical frequency of an output (crystal error and correction applied)."""
        ch = {"CLK0": 1, "CLK1": 2, "CLK2": 3}[src]
        drift = self.bench.xtal_drift_ppm()
        return self.actual(ch) * (1 + (XTAL_PPM + drift) * 1e-6) / (1 + self.corr * 1e-6)

    def running(self, ch):
        if ch == 3 and self.faultiq:
            return False  # build esp32-2432S028R-faultiq: CH3 never starts
        if ch == 3:
            m = self.master
            return self.out[3] and self.out[m] and self.freq[m] >= 4.73e6
        return self.out[ch]

    def condition(self, ch):
        if ch == 3 and self.out[3] and self.faultiq:
            return "ERR"
        if ch == 3 and self.out[3] and not self.running(3):
            return "WAIT"
        return "ON" if self.running(ch) else "OFF"

    def _dropout(self, chans, dur):
        for ch in chans:
            self.bench.events.append((self.bench.now, ch, dur))

    # SCPI -------------------------------------------------------------

    def handle(self, cmd, query):
        u = cmd.upper()
        num = re.search(r"[-+]?\d+(?:\.\d*)?(?:E[-+]?\d+)?", u.split(" ", 1)[1]) if " " in u else None
        val = float(num.group(0)) if num else None
        arg = u.split(" ", 1)[1].strip() if " " in u else ""
        if u == "*IDN?":
            return "CYD,HF-Generator,24:0A:C4:00:00:01,2.1.0-sim"
        if u == "SYST:ERR?":
            return self.errors.pop(0) if self.errors else '0,"No error"'
        if u == "*RST":
            self.reset()
            return ""
        if u == "SYST:SI5351?":
            return "CONN"
        if u == "SYST:HEAP?":
            return "150000,120000,90000"
        if u == "OUTP:REM?":
            return "ALLOWED"
        if u == "SYST:CORR?":
            return "{:.2f}".format(self.corr)
        m = re.match(r"(?:SOUR(\d):)?FREQ(:ACT)?\?$", u)
        if m:
            ch = int(m.group(1) or 1)
            return "{:.1f}".format(self.actual(ch)) if m.group(2) else "{:.0f}".format(self.freq[1 if ch == 3 else ch])
        m = re.match(r"(?:SOUR(\d):)?FREQ ", u)
        if m:
            ch = int(m.group(1) or 1)
            if ch == 3:
                self.errors.append('-221,"CH3 follows its master"')
                return ""
            old = self.freq[ch]
            self.freq[ch] = round(val)
            iq = self.out[3] and self.master == ch
            if iq:
                self._dropout([ch, 3], 0.003)
            elif self.tuning == "LNO" and int(900e6 / old / 2) != int(900e6 / self.freq[ch] / 2):
                self._dropout([ch], 0.0005)
            return ""
        m = re.match(r"OUTP(\d)(:STAT)?\s+(ON|OFF|1|0)$", u)
        if m:
            self.out[int(m.group(1))] = m.group(3) in ("ON", "1")
            return ""
        m = re.match(r"OUTP(\d):COND\?$", u)
        if m:
            return self.condition(int(m.group(1)))
        if u.startswith("SOUR3:IQ:MAST"):
            self.master = int(val)
            return ""
        if u.startswith("SOUR3:PHAS"):
            self.phase = int(val)
            if self.out[3]:
                self._dropout([self.master, 3], 0.003)
            return ""
        if u.startswith("OUTP:DRIV"):
            self.drive = int(val)
            return ""
        if u.startswith("SYST:TUN"):
            self.tuning = arg[:4] if arg.startswith("FRAC") else arg[:3]
            return ""
        if u.startswith("SSC") or u.startswith("SOUR1:SSC"):
            if self.tuning == "INT" and arg != "OFF":
                self.errors.append('-221,"Not in Integer tuning"')
                return ""
            self.ssc = arg
            self._dropout([1] + ([3] if self.out[3] and self.master == 1 else []), 0.002)
            return ""
        if u.startswith("OUTP:OFFS"):
            self.offstate = arg
            return ""
        if u.startswith("SYST:CORR"):
            self.corr = round(val, 2)
            if self.out[3]:
                self._dropout([self.master, 3], 0.003)
            return ""
        if u.startswith("SYST:STEP"):
            self.step = val
            return ""
        if u.startswith("SYST:BACK"):
            self.backlight = int(round(val / 10.0) * 10)
            return ""
        if u.startswith("SOUR2:COUP"):
            self.coupled = arg == "CH1"
            return ""
        if u.startswith("SOUR2:FREQ:OFFS"):
            self.offset = val
            return ""
        if u == "SYST:SAVE":
            return ""
        self.errors.append('-113,"Undefined header"')
        return ""


# =========================================================================== oscilloscope


class SimSds:
    def __init__(self, bench):
        self.bench = bench
        self.ch = {c: {"on": c in (1, 2, 3, 4), "vdiv": 0.2, "ofst": 0.0, "cpl": "D1M", "bwl": False}
                   for c in (1, 2, 3, 4)}
        self.tdiv = 1e-6
        self.msiz = 14e6
        self.trig = ("EDGE", 1)
        self.armed_at = 0.0
        self.t0 = 0.0
        self.t_start = 0.0

    def sara(self):
        on = [c for c in (1, 2, 3, 4) if self.ch[c]["on"]]
        per_adc = {0: 0, 1: 0}
        for c in on:
            per_adc[(c - 1) // 2] += 1
        share = max(1, max(per_adc.values()))
        mem = self.msiz / share
        rate = min(1e9 / share, mem / (self.tdiv * 14))
        if self.tdiv >= 50e-3:  # restart capture: the short simulated record covers the window
            rate = min(rate, SIM_POINTS / (self.tdiv * 14))
        return rate

    def signal(self, src, t, term):
        b = self.bench
        dut = b.dut
        if src is None:
            return np.zeros_like(t)
        if src == "GPS":
            return 0.8 * np.sin(2 * np.pi * 10e6 * t)
        if src == "SDG1":
            f = b.sdg.freq
            p = (f * t) % 1.0
            return self._square(p, f, 0.5, 1.0, 1.75e-9)
        if src == "EN":
            low = [(e[0]) for e in b.events if e[1] == "EN"]
            y = np.full_like(t, 3.3)
            for t_low in low:
                rel = t_low - self.t0
                y[(t >= rel) & (t < rel + 0.1)] = 0.0
            return y
        if src in ("SDA", "SCL"):
            return np.full_like(t, 3.3)
        ch = {"CLK0": 1, "CLK1": 2, "CLK2": 3}[src]
        vpp = VPP_50[dut.drive] * (1.0 if term == 50 else 2.2)
        in_reset = b.last_reset is not None and b.last_reset[0] >= self.armed_at
        running = b.last_reset[1][ch] if in_reset else dut.running(ch)
        if not running:
            lvl = {"LOW": 0.0, "HIGH": (0.12 * dut.drive if term == 50 else 3.3), "HIZ": 0.0}[dut.offstate]
            return np.full_like(t, lvl)
        f = dut.real(src)
        ph = 0.0
        if ch == 3:
            ph = dut.phase / 360.0 - CLK2_SKEW_S * f
        p = (f * t - ph) % 1.0
        y = self._square(p, f, DUTY, vpp, 1.75e-9)
        for (te, ech, dur) in b.events:
            if ech == ch and te >= self.armed_at:
                rel = te - self.t0
                y[(t >= rel) & (t < rel + dur)] = 0.0
        if in_reset:
            y[t >= b.last_reset[0] + 0.33 - self.t0] = 0.0
        return y

    @staticmethod
    def _square(p, f, duty, vpp, rise):
        T = 1.0 / f
        sigma = rise / 2.5  # 1.75 ns: 200 MHz bandwidth of the SDS
        inside = p < duty
        s = np.where(inside, np.minimum(p, duty - p), -np.minimum(p - duty, 1.0 - p)) * T
        return vpp * _phi(s / sigma)

    def waveform(self, c):
        st = self.ch[c]
        sara = self.sara()
        b = self.bench
        self.t0 = self.armed_at
        if self.tdiv >= 0.05:  # reset capture: trigger on EN falling at 10 % of the window
            lows = [e[0] for e in b.events if e[1] == "EN" and e[0] >= self.armed_at]
            if lows:
                self.t0 = lows[-1] - self.tdiv * 14 * 0.1
        n = int(min(self.tdiv * 14 * sara, SIM_POINTS))
        rng = self.bench.rng
        k = np.arange(n)
        t = k / (sara * (1 + SDS_TB_PPM * 1e-6)) + self.t_start + CH_SKEW_S[c]
        t = t + rng.normal(0, 2e-12, n)
        src = self.bench.plugs["scope"].get(c)
        y = self.signal(src, t, self.bench.plugs["term"].get(c, 1e6))
        y = y + rng.normal(0, 0.003, n)
        codes = np.clip(np.round((y + st["ofst"]) * 25.0 / st["vdiv"]), -128, 127).astype(np.int8)
        return codes.tobytes()

    def handle(self, cmd, query):
        u = cmd.upper()
        b = self.bench
        if u == "*IDN?":
            return "Siglent Technologies,SDS1204X-E,SIM0000001,8.2.6.1.37R9"
        if u == "SARA?":
            return "SARA {:.2E}Sa/s".format(self.sara())
        if u == "TDIV?":
            return "TDIV {:.2E}S".format(self.tdiv)
        if u == "MSIZ?":
            return "MSIZ 14M"
        if u == "TRSE?":
            return "TRSE {},SR,C{},HT,OFF".format(*self.trig)
        if u == "SAST?":
            if self.trig[0] == "DROP":
                src = b.plugs["scope"].get(self.trig[1])
                dch = {"CLK0": 1, "CLK1": 2, "CLK2": 3}.get(src)
                hit = any(e[0] >= self.armed_at and e[1] == dch for e in b.events)
                return "SAST Stop" if hit else "SAST Ready"
            return "SAST Stop"
        m = re.match(r"C(\d):(\w+)(\?)?\s*(.*)$", u)
        if m:
            c, key, q, arg = int(m.group(1)), m.group(2), m.group(3), m.group(4)
            st = self.ch[c]
            if key == "VDIV":
                if q:
                    return "C{}:VDIV {:.2E}V".format(c, st["vdiv"])
                st["vdiv"] = float(re.sub("[^0-9.E+-]", "", arg))
            elif key == "OFST":
                if q:
                    return "C{}:OFST {:.2E}V".format(c, st["ofst"])
                st["ofst"] = float(re.sub("[^0-9.E+-]", "", arg))
            elif key == "TRA":
                st["on"] = arg == "ON"
            elif key == "CPL":
                st["cpl"] = arg
            elif key == "WF":
                return _block(self.waveform(c))
            elif key == "PAVA":
                t = self.t_start + np.arange(4000) / 1e8
                src = b.plugs["scope"].get(c)
                y = self.signal(src, t, b.plugs["term"].get(c, 1e6))
                if "MEAN" in arg:
                    return "C{}:PAVA MEAN,{:.3E}V".format(c, float(np.mean(y)))
                return "C{}:PAVA PKPK,{:.3E}V".format(c, float(np.ptp(y)))
            return ""
        if u.startswith("TDIV"):
            self.tdiv = float(re.sub("[^0-9.E+-]", "", u.split(" ", 1)[1]))
            return ""
        if u.startswith("TRSE"):
            parts = u.split(" ", 1)[1].split(",")
            self.trig = ("DROP" if parts[0] == "DROP" else "EDGE", int(parts[2][1:]))
            return ""
        if u in ("TRMD SINGLE", "ARM"):
            self.armed_at = b.now
            self.t_start = 0.0
            return ""
        if u == "SCDP":
            return _block(_bmp())
        return ""


# =========================================================================== spectrum analyzer


class SimSsa:
    def __init__(self, bench):
        self.bench = bench
        self.center = 10e6
        self.span = 1e6
        self.rbw = 1e3
        self.att = 20
        self.det = "POS"
        self.mode = "WRIT"
        self.sweep_time = 1.0
        self.marker_x = 10e6
        self.noise = False
        self.trace_cache = None
        self.swept_at = 0.0

    def lines(self):
        """(frequency, dBm at the analyzer input, kind) of everything at the input."""
        b = self.bench
        dut = b.dut
        src = b.plugs["ssa"]
        if src is None or src not in ("CLK0", "CLK1", "CLK2"):
            return []
        ch = {"CLK0": 1, "CLK1": 2, "CLK2": 3}[src]
        if not dut.running(ch):
            return []
        f0 = dut.real(src)
        out = []
        loss = b.plugs["pad_db"]
        p1 = P1_DBM[dut.drive]
        for n in range(1, 40):
            fn = n * f0
            if fn > 3.2e9:
                break
            from analysis import ideal_square_harmonic_dbc  # noqa
            dbc = ideal_square_harmonic_dbc(n, DUTY)
            if not math.isfinite(dbc):
                continue
            roll = -0.05 * max(0.0, fn / 1e6 - 100.0)
            cable = 0.15 * math.sqrt(fn / 100e6)
            out.append((fn, p1 + dbc + roll - loss - cable, "h"))
        carrier = out[0][1]
        spur = {"FRAC": (-72.0, 18.3e3), "LNO": (-80.0, 31.0e3), "INT": (-88.0, 47.0e3)}[dut.tuning]
        out += [(f0 + spur[1], carrier + spur[0], "s"), (f0 - spur[1], carrier + spur[0] - 3, "s")]
        bl = -85.0 + 5.0 * (dut.backlight / 100.0)
        out += [(f0 + 5e3, carrier + bl, "s"), (f0 - 5e3, carrier + bl, "s")]
        if dut.wlan:
            out += [(f0 + 1.02e3, carrier - 62.0, "s"), (f0 - 1.02e3, carrier - 62.0, "s")]
        other = 2 if ch == 1 else 1
        if dut.out[other]:
            out.append((dut.real({1: "CLK0", 2: "CLK1"}[other]), carrier - 58.0, "x"))
        if ch == 1 and dut.ssc != "OFF":
            out = [(f, p, "spread") if k == "h" and n == 0 else (f, p, k) for n, (f, p, k) in enumerate(out)]
        return out

    def trace(self):
        b = self.bench
        n = 751
        if self.span == 0:
            lines = self.lines()
            lvl = max([p for f, p, k in lines if abs(f - self.center) < self.rbw] + [-120.0])
            y = np.full(n, lvl) + b.rng.normal(0, 0.1, n)
            for (te, ch, dur) in b.events:
                src_ch = {"CLK0": 1, "CLK1": 2, "CLK2": 3}.get(b.plugs["ssa"])
                if ch == src_ch and self.swept_at <= te <= self.swept_at + self.sweep_time + 600:
                    k = min(n - 1, int((te - self.swept_at) % self.sweep_time / self.sweep_time * n))
                    if self.det == "NEG":
                        y[k] = -100.0
            return y
        f = np.linspace(self.center - self.span / 2, self.center + self.span / 2, n)
        danl = -141.0 + 10 * math.log10(self.rbw) + max(0, self.att - 10) * 0.5
        pw = np.full(n, 10 ** (danl / 10.0)) * (1 + 0.3 * b.rng.random(n))
        sigma = self.rbw / 2.355
        for (fl, p, kind) in self.lines():
            if kind == "spread":
                width = abs(self.bench.dut.freq[1] * {"DOWN05": 0.005, "DOWN15": 0.015, "CENT05": 0.01}[self.bench.dut.ssc])
                lo = fl - width / 2
                m = (f >= lo) & (f <= lo + width)
                pw[m] += 10 ** ((p - 10 * math.log10(max(width / self.rbw, 1))) / 10.0)
                continue
            d = f - fl
            shape = np.exp(-0.5 * (d / sigma) ** 2)
            off = np.maximum(np.abs(d), 100.0)
            pn = p - 98.0 - 20 * np.log10(off / 10e3) + 10 * math.log10(self.rbw)
            pn = np.maximum(pn, p - 135.0 + 10 * math.log10(self.rbw))
            pw += 10 ** (p / 10.0) * shape + 10 ** (pn / 10.0)
        return 10 * np.log10(pw)

    def handle(self, cmd, query):
        u = cmd.upper()
        b = self.bench
        val = None
        if " " in u:
            m = re.search(r"[-+]?\d+(?:\.\d*)?(?:E[-+]?\d+)?", u.split(" ", 1)[1])
            val = float(m.group(0)) if m else None
        if u == "*IDN?":
            return "Siglent Technologies,SSA3032X Plus,SIM0000002,3.2.2.5.1R1"
        if u == "*OPC?":
            return "1"
        if u.startswith(":FREQ:CENT"):
            if u.endswith("?"):
                return "{:.6E}".format(self.center)
            self.center = val
        elif u.startswith(":FREQ:SPAN"):
            if u.endswith("?"):
                return "{:.6E}".format(self.span)
            self.span = val
        elif u == ":FREQ:STAR?":
            return "{:.6E}".format(self.center - self.span / 2)
        elif u == ":FREQ:STOP?":
            return "{:.6E}".format(self.center + self.span / 2)
        elif u.startswith(":BWID:VID"):
            pass
        elif u.startswith(":BWID"):
            if u.endswith("?"):
                return "{:.0f}".format(self.rbw)
            self.rbw = val
        elif u.startswith(":POW:ATT"):
            if u.endswith("?"):
                return "{:.0f}".format(self.att)
            if val is not None:
                self.att = val
        elif u.startswith(":DET"):
            self.det = u.split(" ", 1)[1]
        elif u.startswith(":TRAC1:MODE"):
            self.mode = u.split(" ", 1)[1]
        elif u == ":SWE:TIME?":
            return "{:.3f}".format(self.sweep_time)
        elif u.startswith(":SWE:TIME") and val is not None and "AUTO" not in u:
            self.sweep_time = val
        elif u == ":INIT":
            self.swept_at = b.now
            b.sleep(self.sweep_time if self.span == 0 else 0.0)
            self.trace_cache = None
        elif u == ":TRAC:DATA? 1":
            y = self.trace()
            self.trace_cache = y
            return ",".join("{:.2f}".format(v) for v in y)
        elif u == ":CALC:MARK1:MAX":
            y = self.trace_cache if self.trace_cache is not None else self.trace()
            f = np.linspace(self.center - self.span / 2, self.center + self.span / 2, y.size)
            self.marker_x = float(f[int(np.argmax(y))])
        elif u.startswith(":CALC:MARK1:X") and not u.endswith("?"):
            self.marker_x = val
        elif u == ":CALC:MARK1:X?":
            return "{:.3f}".format(self.marker_x)
        elif u == ":CALC:MARK1:Y?":
            y = self.trace()
            f = np.linspace(self.center - self.span / 2, self.center + self.span / 2, y.size)
            k = int(np.argmin(np.abs(f - self.marker_x)))
            v = float(y[k])
            if self.noise:
                v -= 10 * math.log10(self.rbw)
            return "{:.2f}".format(v)
        elif u.startswith(":CALC:MARK1:FUNC"):
            self.noise = "NOIS" in u
        elif u == ":CALC:MARK1:FCO:X?":
            lines = [l for l in self.lines() if abs(l[0] - self.marker_x) < max(self.rbw * 5, 50.0)]
            f = max(lines, key=lambda l: l[1])[0] if lines else self.marker_x
            return "{:.0f}".format(f)
        elif u == ":SENS:ROSC:SOUR?":
            return "EXT"
        elif u == ":HCOP:SDUM:DATA?":
            return _block(_bmp(shade=30))
        elif u.endswith("?"):
            return "0"
        return ""


class SimSdg:
    def __init__(self, bench):
        self.bench = bench
        self.freq = 10e6

    def handle(self, cmd, query):
        u = cmd.upper()
        if u == "*IDN?":
            return "Siglent Technologies,SDG2122X,SIM0000003,2.01.01.37R3"
        if u == "ROSC?":
            return "ROSC EXT"
        m = re.search(r"FRQ,([0-9.E+-]+)", u)
        if m:
            self.freq = float(m.group(1))
        return ""


# =========================================================================== bench


class SimBench:
    def __init__(self, seed=1):
        self.rng = np.random.default_rng(seed)
        self.now = 1000.0
        self.power_on = 0.0
        self.events = []
        self.last_reset = None
        self.plugs = {k: (dict(v) if isinstance(v, dict) else v) for k, v in PLUGS_G.items()}
        self.dut = SimDut(self)
        self.sds = SimSds(self)
        self.ssa = SimSsa(self)
        self.sdg = SimSdg(self)

    def link(self, name):
        handler = {"dut": self.dut.handle, "sds": self.sds.handle, "ssa": self.ssa.handle,
                   "sdg": self.sdg.handle}[name]
        return SimLink(self, handler, name)

    def sleep(self, s):
        self.now += s

    def xtal_drift_ppm(self):
        """Warm-up drift: -0.4 ppm settling with 12 min time constant after power-on."""
        age = max(0.0, self.now - self.power_on)
        return -0.4 * math.exp(-age / 720.0)

    def dut_reset(self):
        """EN low for 0.1 s; the outputs run on until the firmware initialises the SI5351 (0.33 s)."""
        self.events.append((self.now, "EN", 0.1))
        self.last_reset = (self.now, {ch: self.dut.running(ch) for ch in (1, 2, 3)})
        self.now += 0.5
        self.dut.reset()

    def apply_plug(self, plug):
        """Plug variants of MESSPLAN_SIGLENT.md 4 and of the preparation steps."""
        p = {k: (dict(v) if isinstance(v, dict) else v) for k, v in PLUGS_G.items()}
        for name in plug:
            if name == "S1":
                p["scope"][1] = "CLK0"
                p["ssa"] = "CLK1"
            elif name == "S2":
                p["term"][1] = p["term"][3] = 1e6
            elif name == "S3a":
                p["scope"][2] = "EN"
            elif name == "S3b":
                p["scope"][2] = "SDA"
                p["scope"][4] = "SCL"
            elif name == "V03":
                p["scope"][1] = p["scope"][3] = "CLK1"
            elif name == "swapBC":
                p["scope"][1], p["scope"][3] = p["scope"][3], p["scope"][1]
            elif name == "V04a":
                p["scope"][1] = "GPS"
            elif name == "V04b":
                p["scope"][1] = "SDG1"
            elif name == "nopad":
                p["pad_db"] = 0.0
            elif name == "term":
                p["ssa"] = None
            elif name == "CLK2-SSA":
                p["ssa"] = "CLK2"
            elif name == "cold":
                self.power_on = self.now
        self.plugs = p
