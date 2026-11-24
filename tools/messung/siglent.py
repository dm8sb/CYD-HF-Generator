# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Siglent SDS1204X-E, SSA3032X Plus and (optional) SDG2122X over SCPI.

The command strings follow the Siglent programming guides of the SDS1000X-E, SSA3000X Plus and
SDG2000X series. They are collected here and nowhere else; `measure.py check` sends every query
once and lists the ones an instrument does not answer, before the first measurement (V-01).
Commands marked VERIFY are the ones most likely to differ between firmware versions.
"""
import io
import re
import time

import numpy as np

SDS_GRID_DIV = 14  # horizontal divisions of the SDS1000X-E
SDS_CODES_PER_DIV = 25.0  # vertical ADC codes per division (WF? DAT2)


def _float(text):
    """First number in an answer like 'C1:VDIV 2.00E-01V' or 'SARA 1.00E+09Sa/s'."""
    m = re.search(r"[-+]?\d+(?:\.\d*)?(?:[eE][-+]?\d+)?", text.split(" ", 1)[-1] if " " in text else text)
    if not m:
        raise ValueError("no number in '{}'".format(text))
    return float(m.group(0))


def _png_from_image(data):
    """Screenshot bytes (BMP or PNG) as PNG with a reduced palette (small files for the protocol)."""
    from PIL import Image  # Pillow comes with matplotlib

    img = Image.open(io.BytesIO(data)).convert("RGB")
    img = img.quantize(colors=32)
    out = io.BytesIO()
    img.save(out, format="PNG", optimize=True)
    return out.getvalue()


# =========================================================================== SDS1204X-E


class Sds:
    """Oscilloscope. Channels 1 ... 4; channel 4 carries the GPS reference in setup G."""

    def __init__(self, link):
        self.link = link

    def idn(self):
        return self.link.query("*IDN?")

    def write(self, cmd):
        self.link.write(cmd)

    # ------------------------------------------------------------------ setup

    def channel(self, ch, on=True, vdiv=0.2, offset=0.0, coupling="D1M", bwl=False):
        """coupling: D1M (DC 1 MOhm; the 50-Ohm feed-through sits on the input), A1M, GND."""
        c = "C{}".format(ch)
        self.write("{}:TRA {}".format(c, "ON" if on else "OFF"))
        if not on:
            return
        self.write("{}:ATTN 1".format(c))
        self.write("{}:CPL {}".format(c, coupling))
        self.write("{}:VDIV {:.3E}V".format(c, vdiv))
        self.write("{}:OFST {:.3E}V".format(c, offset))
        self.write("BWL {},{}".format(c, "ON" if bwl else "OFF"))

    def only(self, *channels, **kw):
        """Switch on exactly the given channels (others off).

        all={...}: settings for every channel; per={ch: {...}}: settings per channel.
        """
        per = kw.get("per", {})
        common = kw.get("all", {})
        for ch in (1, 2, 3, 4):
            if ch in channels:
                self.channel(ch, True, **per.get(ch, common))
            else:
                self.channel(ch, False)

    def timebase(self, tdiv):
        self.write("TDIV {:.3E}S".format(tdiv))
        self.write("TRDL 0S")

    def memory(self, depth="14M"):
        self.write("MSIZ {}".format(depth))

    def sample_rate(self):
        return _float(self.link.query("SARA?"))

    def trigger_edge(self, ch, level=0.0, slope="POS"):
        c = "C{}".format(ch)
        self.write("TRSE EDGE,SR,{},HT,OFF".format(c))
        self.write("{}:TRSL {}".format(c, slope))
        self.write("{}:TRLV {:.3E}V".format(c, level))

    def trigger_dropout(self, ch, level, timeout_s):
        """Dropout (timeout) trigger: fires when no edge comes for timeout_s. VERIFY syntax."""
        c = "C{}".format(ch)
        self.write("TRSE DROP,SR,{},HT,TI,HV,{:.3E}S".format(c, timeout_s))
        self.write("{}:TRLV {:.3E}V".format(c, level))

    # ------------------------------------------------------------------ acquisition

    def arm_single(self):
        self.write("TRMD SINGLE")
        self.write("ARM")

    def status(self):
        """'Stop' after a single acquisition, 'Ready'/'Arm' while waiting. VERIFY answer text."""
        return self.link.query("SAST?").split(" ", 1)[-1].strip()

    def triggered(self):
        return self.status().lower().startswith("stop")

    def acquire(self, timeout_s=10.0):
        self.arm_single()
        end = time.monotonic() + timeout_s
        time.sleep(0.05)
        while not self.triggered():
            if time.monotonic() > end:
                raise TimeoutError("SDS: no trigger within {} s".format(timeout_s))
            time.sleep(0.05)

    def stop(self):
        self.write("STOP")

    def waveform(self, ch):
        """Raw record of one channel as (volts, dt). All points (WFSU SP,0,NP,0,FP,0)."""
        c = "C{}".format(ch)
        vdiv = _float(self.link.query("{}:VDIV?".format(c)))
        ofst = _float(self.link.query("{}:OFST?".format(c)))
        sara = self.sample_rate()
        self.write("WFSU SP,0,NP,0,FP,0")
        raw = self.link.query_block("{}:WF? DAT2".format(c))
        codes = np.frombuffer(raw, dtype=np.int8).astype(float)
        volts = codes * (vdiv / SDS_CODES_PER_DIV) - ofst
        return volts, 1.0 / sara

    def capture(self, channels, timeout_s=10.0):
        """One single acquisition; returns {ch: volts} and dt (common time base)."""
        self.acquire(timeout_s)
        data = {}
        dt = None
        for ch in channels:
            v, dt = self.waveform(ch)
            data[ch] = v
        return data, dt

    def measure(self, ch, param):
        """Built-in parameter (MEAN, PKPK, FREQ, DUTY ...), for checks only."""
        return _float(self.link.query("C{}:PAVA? {}".format(ch, param)))

    def screenshot_png(self):
        return _png_from_image(self.link.query_block("SCDP"))

    QUERIES = ["*IDN?", "SARA?", "TDIV?", "MSIZ?", "SAST?", "C1:VDIV?", "C1:OFST?", "C4:VDIV?", "TRSE?"]


# =========================================================================== SSA3032X Plus


class Ssa:
    POINTS = 751

    def __init__(self, link):
        self.link = link

    def idn(self):
        return self.link.query("*IDN?")

    def write(self, cmd):
        self.link.write(cmd)

    def opc(self, timeout_s=60.0):
        end = time.monotonic() + timeout_s
        while True:
            try:
                if self.link.query("*OPC?").strip().endswith("1"):
                    return
            except Exception:
                if time.monotonic() > end:
                    raise
            if time.monotonic() > end:
                raise TimeoutError("SSA: sweep not complete")
            time.sleep(0.2)

    # ------------------------------------------------------------------ setup

    def setup(self, center, span, rbw, vbw=None, ref_dbm=0.0, atten_db=None, detector="POS",
              trace_mode="WRIT", average=None, sweep_time=None):
        self.write(":INIT:CONT OFF")
        self.write(":FREQ:CENT {:.6f}Hz".format(center))
        self.write(":FREQ:SPAN {:.3f}Hz".format(span))
        self.write(":BWID {:.0f}Hz".format(rbw))
        self.write(":BWID:VID {:.0f}Hz".format(vbw if vbw else rbw))
        self.write(":DISP:WIND:TRAC:Y:RLEV {:.1f}dBm".format(ref_dbm))
        if atten_db is None:
            self.write(":POW:ATT:AUTO ON")
        else:
            self.write(":POW:ATT {:.0f}".format(atten_db))
        self.write(":POW:GAIN OFF")
        self.write(":DET {}".format(detector))  # POS, NEG, SAMP, AVER ... VERIFY names
        self.write(":TRAC1:MODE {}".format(trace_mode))  # WRIT, MAXH, AVER
        if average:
            self.write(":AVER:TRAC1:COUN {}".format(int(average)))
        if sweep_time:
            self.write(":SWE:TIME {:.3f}s".format(sweep_time))
        else:
            self.write(":SWE:TIME:AUTO ON")

    def attenuation(self, db):
        self.write(":POW:ATT {:.0f}".format(db))

    def sweep(self, count=1, timeout_s=120.0):
        """Single sweep(s); with averaging or max hold `count` sweeps."""
        for _ in range(count):
            self.write(":INIT")
            self.opc(timeout_s)

    def trace(self):
        """Trace 1 as (freqs, dBm); frequency axis from start/stop and the point count."""
        start = _float(self.link.query(":FREQ:STAR?"))
        stop = _float(self.link.query(":FREQ:STOP?"))
        ans = self.link.query(":TRAC:DATA? 1")
        vals = np.array([float(x) for x in ans.replace(";", ",").split(",") if x.strip()])
        if stop > start:
            freqs = np.linspace(start, stop, vals.size)
        else:  # zero span: time axis in seconds instead
            st = _float(self.link.query(":SWE:TIME?"))
            freqs = np.linspace(0.0, st, vals.size)
        return freqs, vals

    def marker_peak(self):
        self.write(":CALC:MARK1:STAT ON")
        self.write(":CALC:MARK1:MAX")
        return _float(self.link.query(":CALC:MARK1:X?")), _float(self.link.query(":CALC:MARK1:Y?"))

    def counter(self, f, span=10e3, rbw=1e3):
        """Marker counter on the carrier near f (1 Hz resolution). VERIFY :FCO syntax."""
        self.setup(f, span, rbw, ref_dbm=0.0)
        self.sweep()
        self.marker_peak()
        self.write(":CALC:MARK1:FCO:STAT ON")
        self.sweep()
        return _float(self.link.query(":CALC:MARK1:FCO:X?"))

    def noise_marker(self, f_carrier, offsets):
        """Noise marker relative to the carrier in dBc/Hz at the given offsets. VERIFY :FUNC."""
        out = []
        _, carrier = self.marker_peak()
        self.write(":CALC:MARK1:FUNC NOIS")
        for off in offsets:
            self.write(":CALC:MARK1:X {:.3f}Hz".format(f_carrier + off))
            out.append((off, _float(self.link.query(":CALC:MARK1:Y?")) - carrier))
        self.write(":CALC:MARK1:FUNC OFF")
        return out

    def reference_source(self):
        """'EXT' when locked to the external 10 MHz. VERIFY: some firmware answers INT/EXT."""
        return self.link.query(":SENS:ROSC:SOUR?").strip()

    def screenshot_png(self):
        return _png_from_image(self.link.query_block(":HCOP:SDUM:DATA?"))  # VERIFY

    QUERIES = ["*IDN?", ":FREQ:CENT?", ":FREQ:SPAN?", ":BWID?", ":POW:ATT?", ":SENS:ROSC:SOUR?",
               ":TRAC:DATA? 1", ":CALC:MARK1:X?"]


# =========================================================================== SDG2122X (optional)


class Sdg:
    """Only for V-04 b: GPS-locked test signal of known frequency."""

    def __init__(self, link):
        self.link = link

    def idn(self):
        return self.link.query("*IDN?")

    def external_reference(self):
        self.link.write("ROSC EXT")
        return self.link.query("ROSC?")

    def square(self, ch, f, vpp=1.0, offset=0.0):
        self.link.write("C{}:BSWV WVTP,SQUARE,FRQ,{:.6f},AMP,{:.3f},OFST,{:.3f},DUTY,50".format(
            ch, f, vpp, offset))

    def sine(self, ch, f, vpp=1.0):
        self.link.write("C{}:BSWV WVTP,SINE,FRQ,{:.6f},AMP,{:.3f},OFST,0".format(ch, f, vpp))

    def output(self, ch, on, load="50"):
        self.link.write("C{}:OUTP {},LOAD,{}".format(ch, "ON" if on else "OFF", load))

    QUERIES = ["*IDN?", "ROSC?", "C1:BSWV?"]
