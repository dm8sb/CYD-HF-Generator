# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""The measurement bench of MESSPLAN_SIGLENT.md 3: generator, SDS, SSA (and optional SDG) with
the shared measurement helpers of section 2 and the plug prompts of section 4.

Setup G: CLK0 -> 20 dB -> SSA; CLK1 -> SDS channel 1, CLK2 -> SDS channel 3 (50-Ohm
feed-throughs); GPS -> SDS channel 4 and SSA Ref In.
"""
import time

import numpy as np

import analysis
from store import INFO

SIG_CH = 1  # CLK1 in setup G (CLK0 with S1)
IQ_CH = 3  # CLK2
GPS_CH = 4
EN_CH = 2

# vertical settings for a CMOS square wave at a 50-Ohm feed-through (0 ... <= 1.5 V)
V50 = {"vdiv": 0.25, "offset": -0.75}
V1M = {"vdiv": 0.5, "offset": -1.65}
VGPS = {"vdiv": 0.25, "offset": 0.0}

PLUG_TEXT = {
    "S1": "S1: swap cables A and B AT THE MODULE (CLK0 -> SDS channel 1, CLK1 -> 20 dB -> SSA).",
    "S2": "S2: pull the 50-Ohm feed-throughs off SDS channels 1 and 3, plug the cables in directly (1 MOhm).",
    "S3a": "S3a: probe 10:1 on SDS channel 2 to EN of the ESP32, ground to GND.",
    "S3b": "S3b: probe on SDS channel 2 to SDA, unplug GPS from channel 4, second probe there to SCL.",
    "V03": "V-03: CLK1 through a BNC T onto two equal cables -> SDS channel 1 and 3 (feed-throughs).",
    "swapBC": "Swap the two cables at the SDS inputs (channel 1 <-> channel 3), feed-throughs stay.",
    "V04a": "V-04 a: second BNC T: GPS also onto SDS channel 1 (feed-through off, 1 MOhm).",
    "V04b": "V-04 b: SDG CH1 output -> SDS channel 1 (feed-through); GPS -> SDG '10MHz In' (rear).",
    "nopad": "V-05: remove the 20 dB attenuator, cable A directly to the SSA (SSA input attenuation is set to 30 dB).",
    "term": "V-06: 50-Ohm termination on the SSA input instead of cable A.",
    "CLK2-SSA": "L6.3: cable C (CLK2) to the 20 dB attenuator of the SSA instead of cable A.",
    "cold": "L5: power the board off for >= 2 h; then power it on and confirm.",
    "G": "Setup G (MESSPLAN_SIGLENT.md 3.1): all variants undone, attenuator and feed-throughs in place.",
}


class Bench:
    def __init__(self, dut, sds, ssa, session, sdg=None, sim=None, record="14M", ask=None):
        self.dut = dut
        self.sds = sds
        self.ssa = ssa
        self.sdg = sdg
        self.s = session
        self.sim = sim
        self.record = record
        self._ask = ask or input
        self.plugged = ["G"]
        self.cal = session.meta.get("cal", {})

    # ------------------------------------------------------------------ time and operator

    def sleep(self, s):
        if self.sim:
            self.sim.sleep(s)
        else:
            time.sleep(s)

    def now(self):
        return self.sim.now if self.sim else time.time()

    def ask(self, text, default=""):
        if self.sim:
            return default
        return self._ask("\n>>> {} ".format(text)).strip()

    def confirm(self, text):
        self.ask(text + "  [Enter]")

    def plug(self, names, text=None):
        """Change the plugs: outputs off, prompt, wait (sim: applied at once)."""
        if isinstance(names, str):
            names = [names]
        try:
            self.dut.all_off()
        except Exception as exc:  # the prompt matters more than a refused OFF
            print("  outputs off failed: {}".format(exc))
        msg = text or " / ".join(PLUG_TEXT.get(n, n) for n in names)
        self.confirm(msg)
        if self.sim:
            self.sim.apply_plug([n for n in names if n != "G"])
        self.plugged = list(names)
        self.s.meta_append("plugs", {"time": self.now(), "plug": names})

    def temperature(self):
        if self.sim:
            return 22.5
        ans = self.ask("Room temperature in degC (empty = skip):")
        try:
            return float(ans.replace(",", "."))
        except ValueError:
            return None

    def photo(self, name, text):
        """Ask for a photo; the operator copies it to img/setup-<name>.jpg."""
        path = self.s.img("setup-{}.jpg".format(name))
        if not self.sim:
            self.confirm("Photo: {}  -> save as {}".format(text, path))
        self.s.meta_append("photos", {"name": name, "file": "setup-{}.jpg".format(name), "text": text})

    # ------------------------------------------------------------------ oscilloscope

    POINTS = {"14K": 14e3, "140K": 140e3, "1.4M": 1.4e6, "14M": 14e6}

    def scope_frequency_setup(self, f, ch=SIG_CH, term50=True, record=None):
        """Channel ch + GPS on channel 4 (different ADCs, 1 GSa/s each), edge trigger on GPS.

        record: memory depth; the default (--record, 14M) gives 14 ms per record, the sweep L1.4
        uses 1.4M because a 14M transfer takes many seconds over the LAN.
        """
        record = record or self.record
        vs = dict(V50 if term50 else V1M)
        vs["bwl"] = f <= 15e6
        self.sds.only(ch, GPS_CH, per={ch: vs, GPS_CH: dict(VGPS, bwl=True)})
        self.sds.memory(record)
        # 1 GSa/s over the whole memory; below 50 kHz a longer window at 100 MSa/s, which still
        # samples the 10 MHz reference 10 times per period (a lower rate would alias it)
        rate = 1e9 if f >= 50e3 else 100e6
        tdiv = self.POINTS[record] / rate / 14.0
        self.sds.timebase(tdiv)
        self.sds.trigger_edge(GPS_CH, 0.0)

    def measure_frequency(self, f_nom, n=1, ch=SIG_CH, raw_name=None, setup=True, record=None):
        """2.1: f of channel ch as ratio to GPS; n records averaged. Returns (f, u)."""
        if setup:
            self.scope_frequency_setup(f_nom, ch, record=record)
        vals = []
        us = []
        for i in range(n):
            data, dt = self.sds.capture([ch, GPS_CH])
            f, u = analysis.frequency_ratio(data[ch], data[GPS_CH], dt, 10e6)
            vals.append(f)
            us.append(u)
            if raw_name and i == 0:
                self.s.save_raw(raw_name, sig=data[ch], ref=data[GPS_CH], dt=dt)
        vals = np.array(vals)
        u_fit = float(np.sqrt(np.mean(np.square(us))))
        u_rep = float(np.std(vals, ddof=1) / np.sqrt(n)) if n > 1 else 0.0
        u_f = self.cal.get("u_f_rel", 0.0) * f_nom
        return float(vals.mean()), max(u_fit, u_rep, u_f)

    def scope_phase_setup(self, f, a=SIG_CH, b=IQ_CH, term50=True):
        vs = dict(V50 if term50 else V1M)
        vs["bwl"] = False
        self.sds.only(a, b, all=vs)
        self.sds.memory("1.4M")
        self.sds.timebase(1.4e6 / 1e9 / 14.0)
        self.sds.trigger_edge(a, vs["offset"] * -1.0)

    def measure_phase(self, f, nominal=None, a=SIG_CH, b=IQ_CH, raw_name=None, setup=True):
        """2.3: phase of channel b behind a, minus the channel offset dt0 of V-03."""
        if setup:
            self.scope_phase_setup(f, a, b)
        data, dt = self.sds.capture([a, b])
        r = analysis.phase(data[a], data[b], dt, f)
        dt0 = self.dt0(f)
        ph = (r["phase"] - dt0 * f * 360.0) % 360.0
        u = max(r["u_phase"], self.cal.get("u_t", 0.0) * f * 360.0)
        if raw_name:
            self.s.save_raw(raw_name, a=data[a], b=data[b], dt=dt)
        out = dict(r, phase=ph, u=u, dt0=dt0, data=data, dt=dt)
        if nominal is not None:
            out["error"] = analysis.phase_error(ph, nominal)
        return out

    def dt0(self, f):
        """Channel offset dt0 from V-03, interpolated over frequency (0 before V-03)."""
        return _interp(self.cal.get("dt0"), f, 0.0)

    def capture_wave(self, channels, f, periods=3, term50=True):
        """Short record for a waveform figure (3 periods, full resolution)."""
        vs = dict(V50 if term50 else V1M)
        self.sds.only(*channels, all=vs)
        tdiv = periods / f / 14.0
        self.sds.memory("14K")
        self.sds.timebase(tdiv)
        self.sds.trigger_edge(channels[0], -vs["offset"])
        data, dt = self.sds.capture(channels)
        return data, dt

    # ------------------------------------------------------------------ spectrum analyzer

    def counter(self, f):
        return self.ssa.counter(f)

    def spectrum(self, name, center, span, rbw, step, mid, title, avg=None, maxhold_sweeps=None,
                 atten=None, ref_dbm=0.0, vbw=None, detector="POS", plot_extra=None, offset_axis=False):
        """Sweep, save the trace (data/<name>.csv with plot spec) and a screenshot."""
        mode = "AVER" if avg else ("MAXH" if maxhold_sweeps else "WRIT")
        self.ssa.setup(center, span, rbw, vbw=vbw, ref_dbm=ref_dbm, atten_db=atten, detector=detector,
                       trace_mode=mode, average=avg)
        self.ssa.sweep(count=(avg or maxhold_sweeps or 1))
        f, p = self.ssa.trace()
        p = p + self.pad_correction(center)
        x = (f - center) if offset_axis else f / 1e6
        plot = {"kind": "spectrum", "title": title, "x": "x", "series": ["level"],
                "xlabel": "Δf / Hz" if offset_axis else "f / MHz", "ylabel": "P / dBm (at the output)"}
        if plot_extra:
            plot.update(plot_extra)
        self.s.save_data(name, {"x": x, "level": p}, plot=plot, step=step, mid=mid,
                         settings="center {:.6g} Hz, span {:.6g} Hz, RBW {:.6g} Hz, avg {}, max hold {}, atten {}".format(
                             center, span, rbw, avg, maxhold_sweeps, atten))
        self.screenshot("ssa", name)
        return f, p

    def pad_correction(self, f):
        """Loss of cable A + 20 dB attenuator from V-05 (dB, added to the analyzer reading)."""
        if "nopad" in self.plugged:
            return 0.0
        return _interp(self.cal.get("pad_loss"), f, 20.0)

    def screenshot(self, dev, name):
        inst = {"ssa": self.ssa, "sds": self.sds}[dev]
        try:
            self.s.save_image("{}_screen.png".format(name), inst.screenshot_png())
        except Exception as exc:  # screenshots are evidence, not a reason to stop a run
            print("  screenshot {} failed: {}".format(dev, exc))

    # ------------------------------------------------------------------ calibration values

    def set_cal(self, **kw):
        self.cal.update(kw)
        self.s.meta_section("cal", **kw)

    def info(self, step, mid, quantity, value, unit="", **kw):
        return self.s.record(step, mid, quantity, value, unit, verdict_=INFO, **kw)

    def manual(self, step, mid, quantity, text):
        """Placeholder for a hand measurement (H); the protocol shows it as an open row."""
        return self.s.record(step, mid, quantity, "", "", note="by hand: " + text)


def _interp(points, x, default):
    """Interpolate a {"<frequency>": value} table (keys as strings, JSON-safe) at x."""
    if not points:
        return default
    items = sorted((float(k), float(v)) for k, v in points.items())
    return float(np.interp(x, [a for a, _ in items], [b for _, b in items]))
