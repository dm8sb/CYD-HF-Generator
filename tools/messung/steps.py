# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Preparation V-01 ... V-07 and runs L1 ... L6 of MESSPLAN_SIGLENT.md (sections 5 and 6).

Each step is a function of the bench, registered with its ID, title and the MESSPLAN.md IDs it
delivers. Limits and verdicts follow MESSPLAN.md; every value goes into results.csv at once,
figures into data/ (drawn by report.py).
"""
import collections
import math

import numpy as np

import analysis
from bench import GPS_CH, IQ_CH, SIG_CH, V1M, V50, VGPS
from store import FAIL, INFO, PASS, UNDECIDED, verdict

Step = collections.namedtuple("Step", "id title mids func group optional")
STEPS = collections.OrderedDict()


def step(sid, title, mids, group=None, optional=False):
    def deco(func):
        STEPS[sid] = Step(sid, title, mids, func, group or sid.split(".")[0].split("-")[0], optional)
        return func
    return deco


class Options:
    """Run options; the dry run and the tests shorten the long loops."""

    def __init__(self, sweep_points=200, drift_minutes=30, warmup_minutes=60, settle_minutes=15,
                 resets=20, quick=False):
        self.sweep_points = sweep_points
        self.drift_minutes = drift_minutes
        self.warmup_minutes = warmup_minutes
        self.settle_minutes = settle_minutes
        self.resets = resets
        self.quick = quick


def _std(b):
    b.dut.standard(b.cal.get("corr"))


def _on(b, ch, f=None):
    if f is not None:
        b.dut.freq(ch, f)
    b.dut.output(ch, True)


# =========================================================================== V: preparation


@step("V-01", "Reference chain: level on SDS channel 4, SSA locked", ["M-003"], "V")
def v01(b, o):
    b.sds.only(GPS_CH, per={GPS_CH: VGPS})
    b.sds.memory("14K")
    b.sds.timebase(100e-9)
    b.sds.trigger_edge(GPS_CH, 0.0)
    data, dt = b.sds.capture([GPS_CH])
    vpp = analysis.vpp(data[GPS_CH])
    v = PASS if 0.36 <= vpp <= 2.0 else FAIL
    b.s.record("V-01", "M-003", "GPS level on the chain", vpp, "Vpp", limit="0.36 ... 2.0", verdict_=v)
    ref = b.ssa.reference_source()
    b.s.record("V-01", "M-003", "SSA reference source", ref, "", verdict_=PASS if "EXT" in ref.upper() else FAIL)
    t = np.arange(data[GPS_CH].size) * dt
    k = slice(0, min(400, t.size))
    b.s.save_data("V-01_gps", {"t_ns": t[k] * 1e9, "gps": data[GPS_CH][k]},
                  plot={"kind": "wave", "title": "GPS reference on SDS channel 4", "x": "t_ns",
                        "series": ["gps"], "xlabel": "t / ns", "ylabel": "U / V"}, step="V-01")
    b.set_cal(gps_vpp=vpp)


@step("V-02", "ADC pairs of the SDS", [], "V")
def v02(b, o):
    b.sds.memory("14M")
    b.sds.timebase(1e-6)
    rates = {}
    for combo in [(1,), (1, 2), (1, 3), (1, 4), (3, 4), (1, 2, 3, 4)]:
        b.sds.only(*combo, all=V50)
        rates[combo] = b.sds.sample_rate()
        b.info("V-02", "", "sample rate channels " + "+".join(map(str, combo)), rates[combo] / 1e6, "MSa/s")
    ok = rates[(1, 3)] >= 0.99e9 and rates[(1, 4)] >= 0.99e9 and rates[(1, 2)] < 0.99e9
    b.s.record("V-02", "", "pairs K1/K2 and K3/K4 (1 GSa/s with K1+K3)", "yes" if ok else "no", "",
               verdict_=PASS if ok else UNDECIDED,
               note="" if ok else "pairing differs: choose two channels on different ADCs for 2.3")
    b.set_cal(adc_pairs_ok=ok)


@step("V-03", "Channel offset dt0 with T split and cable swap", ["M-004"], "V")
def v03(b, o):
    freqs = [10e6, 30e6, 99e6]
    reps = 3 if o.quick else 5
    b.set_cal(dt0={})
    first = {}
    spread = {}
    b.plug("V03")
    _on(b, 2)
    for f in freqs:
        b.dut.freq(2, f)
        b.sleep(1)
        vals = []
        for _ in range(reps):
            r = b.measure_phase(f)
            vals.append(_signed_dt(r["phase"], f))
        first[f] = float(np.mean(vals))
        spread[f] = float(np.std(vals))
    b.plug(["V03", "swapBC"])
    _on(b, 2)
    rows = {"f_MHz": [], "dt0_ps": [], "cable_ps": [], "u_t_ps": []}
    dt0 = {}
    u_t = 0.0
    for f in freqs:
        b.dut.freq(2, f)
        b.sleep(1)
        second = float(np.mean([_signed_dt(b.measure_phase(f)["phase"], f) for _ in range(reps)]))
        d0 = (first[f] + second) / 2.0
        cable = abs(first[f] - second) / 2.0
        ut = math.hypot(spread[f], cable)
        u_t = max(u_t, ut)
        dt0[str(int(f))] = d0
        rows["f_MHz"].append(f / 1e6)
        rows["dt0_ps"].append(d0 * 1e12)
        rows["cable_ps"].append(cable * 1e12)
        rows["u_t_ps"].append(ut * 1e12)
        b.info("V-03", "M-004", "dt0 at {:.0f} MHz".format(f / 1e6), d0 * 1e12, "ps", u=ut * 1e12)
        b.info("V-03", "M-004", "u_phi at {:.0f} MHz".format(f / 1e6), ut * f * 360.0, "deg")
    b.set_cal(dt0=dt0, u_t=u_t)
    b.s.save_data("V-03_dt0", rows, plot={"kind": "scatter", "title": "Channel offset dt0 (SDS K1/K3)",
                                          "x": "f_MHz", "series": ["dt0_ps"], "err": "u_t_ps",
                                          "xlabel": "f / MHz", "ylabel": "dt0 / ps"}, step="V-03")
    b.plug("G")


def _signed_dt(phase_deg, f):
    ph = (phase_deg + 180.0) % 360.0 - 180.0
    return ph / 360.0 / f


@step("V-04", "Frequency method: GPS against GPS, SDS against SSA counter, 160 MHz", ["M-003"], "V")
def v04(b, o):
    n = 3 if o.quick else 10
    b.plug("V04a")
    b.scope_frequency_setup(10e6, term50=False)
    vals = [b.measure_frequency(10e6, setup=False)[0] for _ in range(n)]
    rel = (np.array(vals) - 10e6) / 10e6
    u_gps = max(abs(float(rel.mean())), float(rel.std()))
    b.info("V-04", "M-003", "GPS/GPS ratio deviation (method limit)", u_gps, "rel")
    b.plug("G")
    _std(b)
    for ch in (1, 2):
        _on(b, ch, 10e6)
    b.sleep(10)
    vals = [b.measure_frequency(10e6)[0] for _ in range(n)]
    std_dut = float(np.std(vals) / 10e6)
    f_sds = float(np.mean(vals))
    f_ssa = b.counter(10e6)
    u_f_rel = max(u_gps, std_dut, 1e-10)
    b.set_cal(u_f_rel=u_f_rel)
    b.info("V-04", "M-003", "u_f (relative, used for all frequency values)", u_f_rel, "rel",
           note="std of the DUT at 10 MHz {:.2e}".format(std_dut))
    diff = f_sds - f_ssa
    b.s.record("V-04", "M-003", "SDS (CLK1) - SSA counter (CLK0) at 10 MHz", diff, "Hz", limit=1.0,
               u=u_f_rel * 10e6, verdict_=verdict(diff, u_f_rel * 10e6, 1.0))
    for f in (99.999999e6, 160e6):
        b.dut.freq(2, f)
        b.sleep(2)
        try:
            fm, u = b.measure_frequency(f)
            b.info("V-04", "M-003", "SDS evaluable at {:.6f} MHz".format(f / 1e6), fm - f, "Hz", u=u)
        except analysis.AnalysisError as exc:
            b.s.record("V-04", "M-003", "SDS evaluable at {:.6f} MHz".format(f / 1e6), "no", "",
                       verdict_=FAIL, note=str(exc))


@step("V-04b", "Frequency method against the GPS-locked SDG (optional)", ["M-003"], "V", optional=True)
def v04b(b, o):
    if b.sdg is None:
        b.info("V-04b", "M-003", "skipped", "no SDG", "")
        return
    b.sdg.external_reference()
    b.plug("V04b")
    worst = 0.0
    rows = {"f_MHz": [], "dev_rel": []}
    for f in (8e3, 1e6, 7.1e6, 10e6, 28.4e6, 99.999999e6, 120e6):
        b.sdg.square(1, f, 1.0, 0.5)
        b.sdg.output(1, True)
        b.sleep(2)
        fm, u = b.measure_frequency(f)
        rel = (fm - f) / f
        # only a deviation well beyond the point's own uncertainty is a bias of the method; the
        # larger scatter at 8 kHz (few periods per record) must not inflate u_f at 100 MHz
        worst = max(worst, abs(rel) - 2 * u / f)
        rows["f_MHz"].append(f / 1e6)
        rows["dev_rel"].append(rel)
        b.info("V-04b", "M-003", "SDG {:.6f} MHz: deviation".format(f / 1e6), rel, "rel", u=u / f)
    b.sdg.output(1, False)
    b.set_cal(u_f_rel=max(b.cal.get("u_f_rel", 0.0), worst))
    b.s.save_data("V-04b_sdg", rows, plot={"kind": "scatter", "title": "SDS frequency method against SDG",
                                           "x": "f_MHz", "series": ["dev_rel"], "xscale": "log",
                                           "xlabel": "f / MHz", "ylabel": "relative deviation"}, step="V-04b")
    b.plug("G")


def harmonic_levels(b, f0, ns, atten=None, span=100e3, rbw=1e3):
    """Level (dBm at the output, attenuator loss added) of the given harmonics, narrow sweeps."""
    out = {}
    for n in ns:
        fn = n * f0
        b.ssa.setup(fn, span, rbw, ref_dbm=0.0, atten_db=atten)
        b.ssa.sweep()
        _, lv = b.ssa.marker_peak()
        out[n] = lv + b.pad_correction(fn)
    return out


@step("V-05", "Loss of cable A and 20 dB attenuator from the harmonics of CLK0", ["M-004"], "V")
def v05(b, o):
    _std(b)
    b.dut.drive(8)
    ns = [1, 3, 5, 7, 9, 11, 13, 15]
    b.plug("nopad")
    _on(b, 1, 10e6)
    b.sleep(2)
    direct = harmonic_levels(b, 10e6, ns, atten=30)
    b.plug("G")
    _on(b, 1, 10e6)
    b.sleep(2)
    saved = b.cal.get("pad_loss")
    b.cal["pad_loss"] = {"0": 0.0}  # raw readings through the attenuator
    padded = harmonic_levels(b, 10e6, ns, atten=None)
    loss = {str(int(n * 10e6)): direct[n] - padded[n] for n in ns}
    b.cal["pad_loss"] = saved
    b.set_cal(pad_loss=loss)
    rows = {"f_MHz": [n * 10 for n in ns], "loss_dB": [loss[str(int(n * 10e6))] for n in ns]}
    for f, l in zip(rows["f_MHz"], rows["loss_dB"]):
        b.info("V-05", "M-004", "loss cable A + 20 dB at {:.0f} MHz".format(f), l, "dB")
    b.s.save_data("V-05_loss", rows, plot={"kind": "line", "title": "Loss of cable A and 20 dB attenuator",
                                           "x": "f_MHz", "series": ["loss_dB"], "xlabel": "f / MHz",
                                           "ylabel": "loss / dB"}, step="V-05")


@step("V-06", "SSA noise floor and own distortion", ["M-004"], "V")
def v06(b, o):
    b.plug("term")
    b.ssa.setup(10e6, 200e3, 100, ref_dbm=-20.0, atten_db=0, detector="SAMP", trace_mode="AVER", average=10)
    b.ssa.sweep(count=10)
    f, p = b.ssa.trace()
    danl = float(np.median(p)) - 10 * math.log10(100)
    b.info("V-06", "M-004", "SSA noise floor (RBW 100 Hz, 0 dB atten, normalised)", danl, "dBm/Hz")
    b.plug("G")
    _std(b)
    b.dut.drive(8)
    _on(b, 1, 10e6)
    b.sleep(2)
    table = {}
    for att in (10, 20, 30):
        lv = harmonic_levels(b, 10e6, [1, 2, 3, 4, 5], atten=att)
        table[att] = {n: lv[n] - lv[1] for n in (2, 3, 4, 5)}
        for n in (2, 3, 4, 5):
            b.info("V-06", "M-004", "H{} at {} dB input attenuation".format(n, att), table[att][n], "dBc")
    choice = 30
    for att in (10, 20):
        if all(abs(table[att][n] - table[att + 10][n]) < 1.0 for n in (2, 3, 4, 5)):
            choice = att
            break
    b.set_cal(harm_atten=choice, ssa_danl=danl)
    b.info("V-06", "M-004", "input attenuation without own distortion", choice, "dB")


@step("V-07", "Device under test, warm-up, photos", ["M-001", "M-002", "M-005"], "V")
def v07(b, o):
    info = b.dut.info()
    b.s.set_meta(dut=info)
    for k in ("idn", "si5351", "correction_ppm", "remote_outputs", "heap"):
        b.info("V-07", "M-002", "DUT " + k, info[k], "")
    if info["si5351"] != "CONN" or info["remote_outputs"] != "ALLOWED":
        b.s.record("V-07", "M-002", "DUT ready (SI5351 CONN, remote outputs allowed)", "no", "", verdict_=FAIL)
        raise RuntimeError("device not ready: {}".format(info))
    for name, text in (("overview", "whole setup G from the front"), ("refchain", "reference chain at the rear panels"),
                       ("dut", "module with cables A, B, C"), ("infocard", "CONFIG Info card of the DUT")):
        b.photo(name, text)
    b.s.set_meta(temperature_start=b.temperature())
    _std(b)
    for ch in (1, 2):
        _on(b, ch, 10e6)
    b.sleep(o.settle_minutes * 60)
    b.info("V-07", "M-005", "warm-up with CH1, CH2 = 10 MHz", o.settle_minutes, "min")


# =========================================================================== L1: frequency


@step("L1.1", "Correction 0: crystal error", ["M-107"])
def l11(b, o):
    _std(b)
    b.dut.correction(0.0)
    for ch in (1, 2):
        _on(b, ch, 10e6)
    b.sleep(10)
    f, u = b.measure_frequency(10e6, n=3 if o.quick else 10, raw_name="L1.1_10MHz")
    eps = analysis.ppm(f, 10e6)
    b.info("L1.1", "M-107", "f at 10 MHz, correction 0", f - 10e6, "Hz", u=u, setpoint=10e6)
    b.info("L1.1", "M-107", "crystal error", eps, "ppm")
    b.info("L1.1", "M-107", "SSA counter CLK0 - 10 MHz", b.counter(10e6) - 10e6, "Hz")
    b.info("L1.1", "M-107", "FREQ:ACT? CH2", b.dut.freq_actual(2), "Hz")
    b.set_cal(eps0_ppm=eps)


@step("L1.2", "Calibrate: correction from the crystal error", ["M-108"])
def l12(b, o):
    corr = round(b.cal.get("eps0_ppm", 0.0), 2)
    dev = None
    u = 0.0
    for _ in range(4):
        b.dut.correction(corr)
        b.sleep(2)
        f, u = b.measure_frequency(10e6, n=3 if o.quick else 5)
        dev = f - 10e6
        if abs(dev) < 0.2:
            break
        corr = round(corr + analysis.ppm(f, 10e6), 2)  # positive correction lowers the frequency
    b.set_cal(corr=corr)
    b.s.set_meta(correction_ppm=corr)
    b.s.record("L1.2", "M-108", "deviation at 10 MHz after calibration", dev, "Hz", setpoint=10e6, u=u,
               limit=1.0, verdict_=verdict(dev, u, 1.0))
    b.s.record("L1.2", "M-108", "correction", corr, "ppm", limit=50.0, verdict_=PASS if abs(corr) <= 50 else FAIL)
    b.manual("L1.2", "M-108", "taps and time to calibrate at the device (U-11)", "count taps, stop the time")


BRACKET_POINTS = [(1e6, 0.5, "M-109"), (7.1e6, 0.5, "M-109"), (28.4e6, 0.5, "M-109"),
                  (99.999999e6, 0.5, "M-109"), (8e3, 0.5, "M-111"), (455e3, 0.5, "M-111"),
                  (112.499996e6, 1.5, "M-112"), (112.500004e6, 2.0, "M-112"), (160e6, 3.0, "M-112")]


@step("L1.3", "Bracket measurement over the range", ["M-109", "M-111", "M-112"])
def l13(b, o):
    _std(b)
    for ch in (1, 2):
        _on(b, ch, 10e6)
    rows = {"f_MHz": [], "delta_Hz": [], "u_Hz": [], "limit_Hz": []}
    n = 2 if o.quick else 10
    for f, limit, mid in BRACKET_POINTS:
        b.dut.freq(2, 10e6)
        b.sleep(10)
        f10a, _ = b.measure_frequency(10e6, n=n)
        b.dut.freq(2, f)
        b.sleep(10)
        fm, u = b.measure_frequency(f, n=n)
        act = b.dut.freq_actual(2)
        b.dut.freq(2, 10e6)
        b.sleep(10)
        f10b, _ = b.measure_frequency(10e6, n=n)
        d = analysis.bracket_delta(fm, f, f10a, f10b)
        b.s.record("L1.3", mid, "bracket delta {:.6f} MHz".format(f / 1e6), d, "Hz", setpoint=f, u=u,
                   limit=limit, verdict_=verdict(d, u, limit), note="FREQ:ACT? {:.1f} Hz".format(act))
        rows["f_MHz"].append(f / 1e6)
        rows["delta_Hz"].append(d)
        rows["u_Hz"].append(u)
        rows["limit_Hz"].append(limit)
    b.s.save_data("L1.3_bracket", rows, plot={"kind": "scatter", "title": "Bracket measurement δ (CLK1)",
                                              "x": "f_MHz", "series": ["delta_Hz"], "err": "u_Hz",
                                              "limit_series": "limit_Hz", "xscale": "log",
                                              "xlabel": "f / MHz", "ylabel": "δ / Hz"}, step="L1.3")


@step("L1.4", "Sweep: measured against FREQ:ACT? in all tuning modes", ["M-109"])
def l14(b, o):
    _std(b)
    rng = np.random.default_rng(1)
    for ch in (1, 2):
        _on(b, ch, 10e6)
    worst_all = {}
    for mode in ("FRAC", "LNO", "INT"):
        b.dut.tuning(mode)
        freqs = np.exp(rng.uniform(math.log(8e3), math.log(160e6), o.sweep_points))
        t_ref, f_ref = [], []
        rows = {"f_MHz": [], "act_Hz": [], "dev_Hz": [], "u_Hz": []}
        fails = 0
        for i, f in enumerate(freqs):
            if i % 10 == 0:
                b.dut.freq(2, 10e6)
                b.sleep(5)
                fr, _ = b.measure_frequency(10e6, record="1.4M")
                act10 = b.dut.freq_actual(2)  # 10 MHz is exact (900/90); taken from the device anyway
                t_ref.append(b.now())
                f_ref.append(fr * 10e6 / act10)
            f = round(f)
            b.dut.freq(2, f)
            b.sleep(5)
            act = b.dut.freq_actual(2)
            try:
                fm, u = b.measure_frequency(f, record="1.4M")
            except analysis.AnalysisError as exc:
                b.s.record("L1.4", "M-109", "{} {:.0f} Hz not evaluable".format(mode, f), "", "", verdict_=UNDECIDED,
                           note=str(exc))
                continue
            r = analysis.drift_interpolate(t_ref, f_ref, b.now()) / 10e6
            dev = fm - act * r
            lim = u + 0.05
            if abs(dev) > lim:
                fails += 1
                b.s.record("L1.4", "M-109", "{} {:.0f} Hz: measured - FREQ:ACT?".format(mode, f), dev, "Hz",
                           setpoint=act, u=u, limit=0.05, verdict_=FAIL)
            rows["f_MHz"].append(f / 1e6)
            rows["act_Hz"].append(act)
            rows["dev_Hz"].append(dev)
            rows["u_Hz"].append(u)
        worst = max(abs(x) for x in rows["dev_Hz"]) if rows["dev_Hz"] else float("nan")
        worst_all[mode] = worst
        b.s.record("L1.4", "M-109", "{}: points / deviations over u + 0.05 Hz".format(mode),
                   "{} / {}".format(len(rows["dev_Hz"]), fails), "", verdict_=PASS if fails == 0 else FAIL)
        b.info("L1.4", "M-109", "{}: largest |measured - FREQ:ACT?|".format(mode), worst, "Hz")
        b.s.save_data("L1.4_sweep_{}".format(mode.lower()), rows,
                      plot={"kind": "scatter", "title": "Measured - FREQ:ACT?, tuning {}".format(mode),
                            "x": "f_MHz", "series": ["dev_Hz"], "err": "u_Hz", "xscale": "log",
                            "xlabel": "f / MHz", "ylabel": "Δf / Hz"}, step="L1.4")
    b.dut.tuning("FRAC")


@step("L1.5", "Integer: actual frequency at 7.1 MHz", ["M-110"])
def l15(b, o):
    _std(b)
    b.dut.tuning("INT")
    _on(b, 2, 7.1e6)
    b.sleep(10)
    act = b.dut.freq_actual(2)
    fm, u = b.measure_frequency(act, n=3 if o.quick else 10)
    d = fm - act
    b.s.record("L1.5", "M-110", "Integer 7.1 MHz: measured - FREQ:ACT?", d, "Hz", setpoint=act, u=u,
               limit=0.0, verdict_=verdict(d, u, 0.05 + u))
    b.info("L1.5", "M-110", "FREQ:ACT? (manual: 7.102 273 MHz)", act, "Hz")
    b.manual("L1.5", "M-110", "MAIN shows ~7.100 000 MHz", "look at MAIN")
    b.dut.tuning("FRAC")


@step("L1.6", "Spread on CH1 leaves CH2 unchanged", ["M-422"])
def l16(b, o):
    _std(b)
    _on(b, 1, 10e6)
    _on(b, 2, 7.1e6)
    b.sleep(5)
    before, u = b.measure_frequency(7.1e6, n=3)
    b.dut.ssc("DOWN05")
    b.sleep(5)
    after, u2 = b.measure_frequency(7.1e6, n=3)
    d = after - before
    uu = math.hypot(u, u2)
    b.s.record("L1.6", "M-422", "CH2 7.1 MHz with spread -0.5 % on CH1: change", d, "Hz", u=uu, limit=0.0,
               verdict_=PASS if abs(d) <= 2 * uu + 0.01 else FAIL)
    b.dut.ssc("OFF")


# =========================================================================== L2: dropouts


class Watch:
    """Dropout watch (2.4): SDS dropout trigger on one channel, SSA zero span on CLK0."""

    def __init__(self, b, f_min, scope_ch=SIG_CH, ssa_f=None, sweep_s=10.0, channels=None):
        self.b = b
        self.ch = scope_ch
        self.f_min = f_min
        self.timeout = 3.0 / f_min
        self.hits = []
        self.ssa_f = ssa_f
        chans = channels or [scope_ch]
        b.sds.only(*chans, all=V50)
        b.sds.memory("1.4M")
        b.sds.timebase(1e-3)
        b.sds.trigger_dropout(scope_ch, -V50["offset"], self.timeout)
        self.channels = chans
        if ssa_f:
            b.ssa.setup(ssa_f, 0, 1e6, ref_dbm=0.0, detector="NEG", sweep_time=sweep_s)
            b.ssa.write(":INIT")
        b.sds.arm_single()

    def check(self, label):
        b = self.b
        if not b.sds.triggered():
            return False
        dur = None
        try:
            data = {c: b.sds.waveform(c)[0] for c in self.channels}
            dt = b.sds.waveform(self.ch)[1]
            g = analysis.gaps(data[self.ch], dt, self.timeout)
            dur = max(x[1] for x in g) if g else None
        except Exception as exc:  # a failed download must not hide the dropout itself
            print("  dropout download failed: {}".format(exc))
        self.hits.append((label, dur))
        b.sds.arm_single()
        return True

    def ssa_dips(self):
        if not self.ssa_f:
            return None
        self.b.ssa.opc()
        _, p = self.b.ssa.trace()
        ref = float(np.median(p))
        return int(np.sum(p < ref - 10.0))


def _tune(b, w, ch, seq, label, pause=0.0):
    for f in seq:
        b.dut.freq(ch, f)
        if pause:
            b.sleep(pause)
        w.check("{} {:.0f} Hz".format(label, f))


def _seq_steps(start, step_hz, n):
    return [start + step_hz * (i + 1) for i in range(n)]


@step("L2.1", "Tune CH2, CH1 stays undisturbed", ["M-113"])
def l21(b, o):
    _std(b)
    _on(b, 1, 10e6)
    _on(b, 2, 7.1e6)
    b.sleep(2)
    rng = np.random.default_rng(2)
    seq = (_seq_steps(7.1e6, 1e6, 20) + _seq_steps(27.1e6, 1e3, 20) + _seq_steps(27.12e6, 1, 20)
           + [float(round(x)) for x in rng.uniform(1e6, 99e6, 20)])
    w = Watch(b, 1e6, ssa_f=10e6, sweep_s=max(10.0, len(seq) * 0.2))
    _tune(b, w, 2, seq, "CH2")
    dips = w.ssa_dips()
    b.s.record("L2.1", "M-113", "dropouts of CH1 (SSA zero span) while CH2 is tuned", dips, "",
               limit=0, verdict_=PASS if dips == 0 else FAIL, note="{} steps".format(len(seq)))
    b.info("L2.1", "M-113", "dropouts of CH2 itself (SDS)", len(w.hits), "",
           note="; ".join("{} {}".format(l, _us(d)) for l, d in w.hits))


def _us(d):
    return "" if d is None else "{:.1f} µs".format(d * 1e6)


@step("L2.2", "Tune CH1, CH2 stays undisturbed", ["M-113"])
def l22(b, o):
    _std(b)
    _on(b, 2, 10e6)
    _on(b, 1, 7.1e6)
    b.sleep(2)
    rng = np.random.default_rng(3)
    seq = (_seq_steps(7.1e6, 1e6, 20) + _seq_steps(27.1e6, 1e3, 20) + _seq_steps(27.12e6, 1, 20)
           + [float(round(x)) for x in rng.uniform(1e6, 99e6, 20)])
    w = Watch(b, 10e6)
    _tune(b, w, 1, seq, "CH1")
    b.s.record("L2.2", "M-113", "dropouts of CH2 (SDS) while CH1 is tuned", len(w.hits), "", limit=0,
               verdict_=PASS if not w.hits else FAIL, note="{} steps".format(len(seq)))


def _continuous_tuning(b, ch, step_name):
    _std(b)
    _on(b, ch, 10e6)
    b.sleep(2)
    w = Watch(b, 10e6)
    b.dut.step(1)
    n = 50 if b.sim else 200
    _tune(b, w, ch, _seq_steps(10e6, 1, n), "1 Hz", pause=0.04)
    first = len(w.hits)
    b.dut.freq(ch, 95e6)
    b.sleep(1)
    w2 = Watch(b, 95e6)
    _tune(b, w2, ch, _seq_steps(95e6, 1e6, 10), "1 MHz")
    ok = first == 0 and len(w2.hits) <= 1
    b.s.record(step_name, "M-114", "dropouts CH{}: 1-Hz steps / 1-MHz steps 95 ... 105 MHz".format(ch),
               "{} / {}".format(first, len(w2.hits)), "", verdict_=PASS if ok else FAIL,
               note="allowed: one dropout at a PLL change (A-206); " +
                    "; ".join("{} {}".format(l, _us(d)) for l, d in w2.hits))


@step("L2.3", "Continuous tuning in Fractional (CH2)", ["M-114"])
def l23(b, o):
    _continuous_tuning(b, 2, "L2.3")


@step("L2.4", "Correction +1 ppm: frequency step without dropout", ["M-114"])
def l24(b, o):
    _std(b)
    _on(b, 2, 10e6)
    b.sleep(2)
    corr = b.cal.get("corr", 0.0)
    f0, u0 = b.measure_frequency(10e6, n=3)
    w = Watch(b, 10e6)
    b.dut.correction(corr + 1.0)
    b.sleep(1)
    w.check("corr +1 ppm")
    f1, u1 = b.measure_frequency(10e6, n=3)
    b.dut.correction(corr)
    d = f1 - f0
    uu = math.hypot(u0, u1)
    b.s.record("L2.4", "M-114", "frequency change with correction +1.00 ppm", d, "Hz", setpoint=-10.0, u=uu,
               limit=0.5, verdict_=verdict(d + 10.0, uu, 0.5))
    b.s.record("L2.4", "M-114", "dropouts at the correction change", len(w.hits), "", limit=0,
               verdict_=PASS if not w.hits else FAIL)


@step("L2.5", "Tuning in Low noise", ["M-303"])
def l25(b, o):
    _std(b)
    b.dut.tuning("LNO")
    _on(b, 2, 9.99e6)
    b.sleep(2)
    w = Watch(b, 7e6)
    _tune(b, w, 2, _seq_steps(9.99e6, 1e3, 20), "LNO")
    b.dut.freq(2, 7.0e6)
    b.sleep(1)
    w.check("jump 7.0 MHz")
    w.hits = [h for h in w.hits if not h[0].startswith("jump")]
    _tune(b, w, 2, _seq_steps(7.0e6, 10e3, 20), "LNO")
    b.s.record("L2.5", "M-303", "dropouts in Low noise", len(w.hits), "", verdict_=INFO,
               note="expected one when crossing 10.000 MHz (divider 90 -> 88): " +
                    "; ".join("{} {}".format(l, _us(d)) for l, d in w.hits))
    b.dut.tuning("FRAC")


@step("L2.6", "IQ: tuning the master realigns (dropout of both outputs)", ["M-304"])
def l26(b, o):
    _std(b)
    _on(b, 2, 10e6)
    b.dut.iq(2, 90)
    b.dut.output(3, True)
    b.sleep(2)
    w = Watch(b, 10e6, channels=[SIG_CH, IQ_CH])
    f = 10e6
    for i in range(10):
        f += 1e3 if i % 2 == 0 else -1e3
        b.dut.freq(2, f)
        b.sleep(0.5)
        w.check("master {:.0f} Hz".format(f))
    durs = [d for _, d in w.hits if d is not None]
    b.info("L2.6", "M-304", "dropouts at 10 master steps", len(w.hits), "")
    if durs:
        b.info("L2.6", "M-304", "dropout duration min / max", "{:.0f} / {:.0f}".format(min(durs) * 1e6,
                                                                                     max(durs) * 1e6), "µs")


@step("L2.7", "Spread on/off: dropout of CH1", [])
def l27(b, o):
    _std(b)
    _on(b, 1, 10e6)
    b.sleep(2)
    b.ssa.setup(10e6, 0, 1e6, ref_dbm=0.0, detector="NEG", sweep_time=2.0)
    b.ssa.write(":INIT")
    b.dut.ssc("DOWN05")
    b.sleep(0.5)
    b.dut.ssc("OFF")
    b.ssa.opc()
    t, p = b.ssa.trace()
    ref = float(np.median(p))
    dips = int(np.sum(p < ref - 10))
    b.info("L2.7", "", "zero-span points below -10 dB at spread on/off (2 s / 751 points)", dips, "",
           note="point width {:.1f} ms".format(2.0 / 751 * 1e3))


# =========================================================================== L3: IQ phase and waveforms


def _iq(b, f, master=2):
    _std(b)
    _on(b, master, f)
    b.dut.iq(master, 0)
    b.dut.output(3, True)
    b.sleep(2)


def _phases(b, sid, mid, f, phases=(0, 90, 180, 270), n=5, store=True):
    out = {}
    b.scope_phase_setup(f)
    for ph in phases:
        b.dut.write("SOUR3:PHAS {}".format(ph))
        b.sleep(2)
        vals = [b.measure_phase(f, ph, setup=False) for _ in range(n)]
        err = float(np.mean([v["error"] for v in vals]))
        u = max(max(v["u"] for v in vals), float(np.std([v["error"] for v in vals])))
        out[ph] = (err, u, vals[0])
        if store:
            b.s.record(sid, mid, "phase {}° at {:.3f} MHz: error".format(ph, f / 1e6), err, "deg",
                       setpoint=ph, u=u, limit=1.0, verdict_=verdict(err, u, 1.0))
    pr = b.s.meta.get("phase_results", {})
    pr.setdefault(str(int(f)), {}).update({str(k): v[0] for k, v in out.items()})
    b.s.set_meta(phase_results=pr)
    return out


@step("L3.1", "Four phases at 10 MHz", ["M-115"])
def l31(b, o):
    _iq(b, 10e6)
    res = _phases(b, "L3.1", "M-115", 10e6, n=2 if o.quick else 5)
    cols = {}
    for ph, (_, _, v) in res.items():
        a = v["data"][SIG_CH]
        c = v["data"][IQ_CH]
        k = int(round(3.0 / 10e6 / v["dt"]))
        if not cols:
            cols["t_ns"] = np.arange(k) * v["dt"] * 1e9
        cols["clk1_{}".format(ph)] = a[:k]
        cols["clk2_{}".format(ph)] = c[:k]
    b.s.save_data("L3.1_iq_waves", cols, step="L3.1", mid="M-115",
                  plot={"kind": "wave", "title": "CLK1 (master) and CLK2 at 10 MHz", "x": "t_ns",
                        "subplots": [["clk1_{}".format(p), "clk2_{}".format(p)] for p in (0, 90, 180, 270)],
                        "subtitles": ["0°", "90°", "180°", "270°"], "xlabel": "t / ns", "ylabel": "U / V"})


@step("L3.2", "Phases at 4.73, 14.2 and 99 MHz (99 MHz with cable swap)", ["M-116"])
def l32(b, o):
    n = 2 if o.quick else 5
    for f in (4.73e6, 14.2e6):
        _iq(b, f)
        _phases(b, "L3.2", "M-116", f, n=n)
    f = 99e6
    _iq(b, f)
    a = _phases(b, "L3.2", "M-116", f, n=n, store=False)
    b.plug(["swapBC"])
    _iq(b, f)
    sw = {}
    b.scope_phase_setup(f)
    for ph in (0, 90, 180, 270):
        b.dut.write("SOUR3:PHAS {}".format(ph))
        b.sleep(2)
        vals = [b.measure_phase(f, setup=False) for _ in range(n)]
        sw[ph] = float(np.mean([analysis.phase_error(v["phase"], -ph) for v in vals]))
    b.plug("G")
    pr = b.s.meta.get("phase_results", {})
    for ph in (0, 90, 180, 270):
        ea, ua, _ = a[ph]
        offset = (ea + sw[ph]) / 2.0
        err = ea - offset
        u = max(ua, b.cal.get("u_t", 0.0) * f * 360.0 / 4)
        b.s.record("L3.2", "M-116", "phase {}° at 99 MHz (cable swap): error".format(ph), err, "deg", setpoint=ph,
                   u=u, limit=1.0, verdict_=verdict(err, u, 1.0) if u <= 0.5 else UNDECIDED,
                   note="offset of the setup {:.2f}°".format(offset))
        pr.setdefault(str(int(f)), {})[str(ph)] = err
    b.s.set_meta(phase_results=pr)


@step("L3.3", "Output skew at 0° over frequency", ["M-307"])
def l33(b, o):
    for f in (30e6, 50e6):
        _iq(b, f)
        _phases(b, "L3.3", "M-307", f, phases=(0,), n=2 if o.quick else 5)
    pr = b.s.meta.get("phase_results", {})
    rows = {"f_MHz": [], "skew_ps": [], "err0_deg": []}
    for fk in sorted(pr, key=float):
        if "0" in pr[fk]:
            f = float(fk)
            rows["f_MHz"].append(f / 1e6)
            rows["err0_deg"].append(pr[fk]["0"])
            rows["skew_ps"].append(pr[fk]["0"] / 360.0 / f * 1e12)
    worst = max(abs(x) for x in rows["err0_deg"]) if rows["err0_deg"] else float("nan")
    b.s.record("L3.3", "M-307", "largest phase error at 0° up to 99 MHz", worst, "deg", limit=1.0,
               verdict_=PASS if worst <= 1.0 else FAIL, note="else skew compensation (FW-04)")
    b.s.save_data("L3.3_skew", rows, plot={"kind": "scatter", "title": "Output skew CLK2 - CLK1 at 0°",
                                           "x": "f_MHz", "series": ["skew_ps"], "xlabel": "f / MHz",
                                           "ylabel": "Δt / ps"}, step="L3.3")


@step("L3.4", "180°/270° with odd divider (evaluation)", ["M-308"])
def l34(b, o):
    pr = b.s.meta.get("phase_results", {})

    def worst(fk):
        d = pr.get(fk, {})
        return max(abs(d.get("180", 0.0)), abs(d.get("270", 0.0)))

    even = worst(str(int(10e6)))
    odd = max(worst(str(int(14.2e6))), worst(str(int(99e6))))
    b.s.record("L3.4", "M-308", "odd divider error - even divider error (180°/270°)", odd - even, "deg",
               limit=1.0, verdict_=PASS if odd - even <= 1.0 else FAIL, note="else FW-03")


@step("L3.5", "Repeatability after realignment", ["M-306"])
def l35(b, o):
    _iq(b, 10e6)
    b.dut.write("SOUR3:PHAS 90")
    b.scope_phase_setup(10e6)
    f = 10e6
    vals = []
    for i in range(10):
        f += 1e3 if i % 2 == 0 else -1e3
        b.dut.freq(2, f)
        b.sleep(2)
        vals.append(b.measure_phase(f, 90, setup=False)["error"])
    m, s = float(np.mean(vals)), float(np.std(vals))
    u = b.cal.get("u_t", 0.0) * 10e6 * 360.0
    b.s.record("L3.5", "M-306", "90° after 10 realignments: mean error", m, "deg", u=u, limit=1.0,
               verdict_=verdict(m, u, 1.0))
    b.s.record("L3.5", "M-306", "90° after 10 realignments: spread (σ)", s, "deg", limit=0.5,
               verdict_=PASS if s < 0.5 else FAIL)


@step("L3.6", "Duty cycle of CLK1 and CLK2", ["M-309"])
def l36(b, o):
    for ch, f in ((SIG_CH, 7.1e6), (SIG_CH, 10e6), (IQ_CH, 14.2e6), (IQ_CH, 99e6)):
        _iq(b, f)
        b.scope_phase_setup(f)
        data, dt = b.sds.capture([SIG_CH, IQ_CH])
        d = analysis.duty_cycle(data[ch], dt)
        name = "CLK1" if ch == SIG_CH else "CLK2"
        b.s.record("L3.6", "M-309", "duty cycle {} at {:.1f} MHz".format(name, f / 1e6), d, "%", setpoint=50.0,
                   verdict_=INFO, note="> 2 % off: refine manual and A-201" if abs(d - 50) > 2 else "")


@step("L3.7", "Spread on CH1 with IQ on CH2", ["M-423"])
def l37(b, o):
    _iq(b, 10e6)
    b.dut.write("SOUR3:PHAS 90")
    _on(b, 1, 10e6)
    b.sleep(2)
    before = b.measure_phase(10e6, 90)["error"]
    b.dut.ssc("CENT05")
    b.sleep(2)
    after = b.measure_phase(10e6, 90)["error"]
    u = b.cal.get("u_t", 0.0) * 10e6 * 360.0
    b.s.record("L3.7", "M-423", "IQ on CH2: phase change with spread on CH1", after - before, "deg", u=u,
               limit=1.0, verdict_=verdict(after - before, u, 1.0))
    b.dut.ssc("OFF")


@step("L3.8", "Phase drift over 30 min", ["M-412"])
def l38(b, o):
    _iq(b, 10e6)
    b.dut.write("SOUR3:PHAS 90")
    b.scope_phase_setup(10e6)
    t0 = b.now()
    rows = {"t_min": [], "phase_err_deg": []}
    temp0 = b.temperature()
    for _ in range(o.drift_minutes + 1):
        rows["t_min"].append((b.now() - t0) / 60.0)
        rows["phase_err_deg"].append(b.measure_phase(10e6, 90, setup=False)["error"])
        b.sleep(60)
    temp1 = b.temperature()
    span = max(rows["phase_err_deg"]) - min(rows["phase_err_deg"])
    b.s.record("L3.8", "M-412", "phase drift (max - min) in {} min".format(o.drift_minutes), span, "deg",
               limit=0.5, verdict_=PASS if span < 0.5 else FAIL,
               note="room temperature {} -> {} °C".format(temp0, temp1))
    b.s.save_data("L3.8_drift", rows, plot={"kind": "line", "title": "IQ phase error at 10 MHz, 90°",
                                            "x": "t_min", "series": ["phase_err_deg"], "xlabel": "t / min",
                                            "ylabel": "error / deg"}, step="L3.8")


@step("L3.9", "S1: master CH1 (phases, spread with IQ, duty cycle CLK0)", ["M-115", "M-116", "M-423", "M-309"])
def l39(b, o):
    b.plug("S1")
    n = 2 if o.quick else 5
    for f, mid in ((10e6, "M-115"), (99e6, "M-116")):
        _iq(b, f, master=1)
        _phases(b, "L3.9", mid, f, n=n, store=True)
    _iq(b, 10e6, master=1)
    b.dut.write("SOUR3:PHAS 90")
    before = b.measure_phase(10e6, 90)["error"]
    b.dut.ssc("CENT05")
    b.sleep(2)
    after = b.measure_phase(10e6, 90)["error"]
    u = b.cal.get("u_t", 0.0) * 10e6 * 360.0
    b.s.record("L3.9", "M-423", "IQ master CH1: phase change with spread ±0.5 %", after - before, "deg", u=u,
               limit=1.0, verdict_=verdict(after - before, u, 1.0))
    b.dut.ssc("OFF")
    for f in (7.1e6, 10e6):
        _iq(b, f, master=1)
        b.scope_phase_setup(f)
        data, dt = b.sds.capture([SIG_CH, IQ_CH])
        b.s.record("L3.9", "M-309", "duty cycle CLK0 at {:.1f} MHz".format(f / 1e6),
                   analysis.duty_cycle(data[SIG_CH], dt), "%", setpoint=50.0, verdict_=INFO)
    b.plug("G")


@step("L3.10", "Waveform and level per drive at 50 Ω", ["M-312"])
def l310(b, o):
    _std(b)
    _on(b, 2, 10e6)
    cols = {}
    for ma in (2, 4, 6, 8):
        b.dut.drive(ma)
        b.sleep(1)
        data, dt = b.capture_wave([SIG_CH], 10e6, periods=3)
        y = data[SIG_CH]
        if not cols:
            cols["t_ns"] = np.arange(y.size) * dt * 1e9
        cols["{}mA".format(ma)] = y
        b.info("L3.10", "M-312", "Vpp at 50 Ω, 10 MHz, {} mA".format(ma), analysis.vpp(y), "V")
        try:
            b.info("L3.10", "M-312", "rise time 10-90 %, {} mA".format(ma), analysis.rise_time(y, dt) * 1e9, "ns")
        except analysis.AnalysisError:
            pass
    b.s.save_data("L3.10_drive", cols, plot={"kind": "wave", "title": "CLK1 at 50 Ω, 10 MHz, per drive",
                                             "x": "t_ns", "series": ["2mA", "4mA", "6mA", "8mA"],
                                             "xlabel": "t / ns", "ylabel": "U / V"}, step="L3.10", mid="M-312")


# =========================================================================== L4: spectrum


def _carrier_and_spurs(f, p, fc, exclude):
    carrier, sp = analysis.spurs(f, p, fc, exclude, n=3)
    return carrier, sp


def _noise_at(f, p, fc, offset, rbw):
    """Level at fc + offset relative to the carrier, normalised to 1 Hz (dBc/Hz, approximate)."""
    _, carrier = analysis.peak(f, p, fc, 3 * rbw)
    k = int(np.argmin(np.abs(f - (fc + offset))))
    return float(p[k] - carrier - 10 * math.log10(rbw))


@step("L4.1", "Tuning modes compared at 7.1 MHz", ["M-301"])
def l41(b, o):
    cols = {}
    for mode in ("FRAC", "LNO", "INT"):
        _std(b)
        b.dut.tuning(mode)
        _on(b, 1, 7.1e6)
        b.sleep(2)
        fc = b.dut.freq_actual(1) * (1 + b.cal.get("eps0_ppm", 0.0) * 0 * 1e-6)
        f, p = b.spectrum("L4.1_{}".format(mode.lower()), fc, 200e3, 100, "L4.1", "M-301",
                          "CLK0 7.1 MHz, tuning {}".format(mode), avg=10, offset_axis=True)
        fpk, _ = analysis.peak(f, p, fc, 2e3)
        carrier, sp = _carrier_and_spurs(f, p, fpk, 1e3)
        top = sp[0] if sp else (float("nan"), float("nan"))
        b.info("L4.1", "M-301", "{}: largest spur".format(mode), top[1], "dBc", note="at {:+.0f} Hz".format(top[0]))
        b.info("L4.1", "M-301", "{}: noise at 1 kHz offset".format(mode), _noise_at(f, p, fpk, 1e3, 100), "dBc/Hz")
        b.info("L4.1", "M-301", "{}: FREQ:ACT? - 7.1 MHz".format(mode), b.dut.freq_actual(1) - 7.1e6, "Hz")
        if "x" not in cols:
            cols["x"] = f - fpk
        cols[mode.lower()] = p
    b.s.save_data("L4.1_tuning", cols, plot={"kind": "spectrum", "title": "CLK0 7.1 MHz: tuning modes",
                                             "x": "x", "series": ["frac", "lno", "int"],
                                             "xlabel": "Δf / Hz", "ylabel": "P / dBm"}, step="L4.1", mid="M-301")
    b.dut.tuning("FRAC")


@step("L4.2", "Spurs: 10 MHz integer, 7.1 MHz with CH3 (correction 0)", ["M-302"])
def l42(b, o):
    for name, f, iq in (("a_10MHz", 10e6, False), ("c_7.1MHz_iq", 7.1e6, True)):
        _std(b)
        b.dut.correction(0.0)
        _on(b, 1, f)
        if iq:
            b.dut.iq(1, 90)
            b.dut.output(3, True)
        b.sleep(2)
        fs, p = b.spectrum("L4.2_{}".format(name), f, 200e3, 100, "L4.2", "M-302", "CLK0 {}".format(name),
                           avg=10, offset_axis=True)
        fpk, _ = analysis.peak(fs, p, f, 2e3)
        _, sp = _carrier_and_spurs(fs, p, fpk, 1e3)
        for off, dbc in sp:
            b.info("L4.2", "M-302", "{}: spur".format(name), dbc, "dBc", note="at {:+.0f} Hz".format(off))
    b.dut.correction(b.cal.get("corr", 0.0))


@step("L4.3", "Integer to fractional: 10 MHz with correction 0 and +0.01 ppm", ["M-415"])
def l43(b, o):
    for corr in (0.0, 0.01):
        _std(b)
        b.dut.correction(corr)
        _on(b, 1, 10e6)
        b.sleep(2)
        fs, p = b.spectrum("L4.3_corr_{:+.2f}".format(corr), 10e6, 200e3, 100, "L4.3", "M-415",
                           "CLK0 10 MHz, correction {:+.2f} ppm".format(corr), avg=10, offset_axis=True)
        fpk, _ = analysis.peak(fs, p, 10e6, 2e3)
        _, sp = _carrier_and_spurs(fs, p, fpk, 1e3)
        top = sp[0] if sp else (float("nan"), float("nan"))
        b.info("L4.3", "M-415", "correction {:+.2f} ppm: largest spur".format(corr), top[1], "dBc",
               note="at {:+.0f} Hz".format(top[0]))
    b.dut.correction(b.cal.get("corr", 0.0))


def _maxhold(b, name, step_id, mid, title, seconds, center=10e6, span=20e3, rbw=10):
    sweeps = 2 if b.sim else max(1, int(seconds / 6))
    return b.spectrum(name, center, span, rbw, step_id, mid, title, maxhold_sweeps=sweeps, offset_axis=True)


@step("L4.4", "Platform: backlight, 2-s supervision", ["M-310"])
def l44(b, o):
    _std(b)
    _on(b, 1, 10e6)
    b.sleep(2)
    cols = {}
    for pct in (100, 50, 10):
        b.dut.backlight(pct)
        b.sleep(1)
        f, p = _maxhold(b, "L4.4_backlight_{}".format(pct), "L4.4", "M-310",
                        "CLK0 10 MHz, backlight {} %".format(pct), 60)
        fpk, _ = analysis.peak(f, p, 10e6, 50)
        _, sp = _carrier_and_spurs(f, p, fpk, 200)
        for off, dbc in sp[:2]:
            b.info("L4.4", "M-310", "backlight {} %: spur".format(pct), dbc, "dBc", note="at {:+.0f} Hz".format(off))
        if "x" not in cols:
            cols["x"] = f - fpk
        cols["bl{}".format(pct)] = p
    b.s.save_data("L4.4_backlight", cols, plot={"kind": "spectrum", "title": "CLK0 10 MHz: backlight",
                                                "x": "x", "series": ["bl100", "bl50", "bl10"],
                                                "xlabel": "Δf / Hz", "ylabel": "P / dBm"}, step="L4.4", mid="M-310")
    b.dut.backlight(80)
    b.ssa.setup(10e6, 0, 1e3, ref_dbm=0.0, detector="SAMP", sweep_time=10.0)
    b.ssa.sweep()
    t, p = b.ssa.trace()
    b.info("L4.4", "M-310", "zero span 10 s on the carrier: max - min", float(np.ptp(p)), "dB",
           note="2-s supervision visible if periodic")
    b.s.save_data("L4.4_zerospan", {"t_s": t, "level": p},
                  plot={"kind": "line", "title": "Zero span on the carrier (RBW 1 kHz)", "x": "t_s",
                        "series": ["level"], "xlabel": "t / s", "ylabel": "P / dBm"}, step="L4.4", mid="M-310")


@step("L4.5", "Platform: redraw while CH2 changes 10 times per second", ["M-310"])
def l45(b, o):
    _std(b)
    _on(b, 1, 10e6)
    _on(b, 2, 7.1e6)
    b.sleep(2)
    _maxhold(b, "L4.5_quiet", "L4.5", "M-310", "CLK0 10 MHz, GUI quiet", 30)
    b.ssa.setup(10e6, 20e3, 10, ref_dbm=0.0, trace_mode="MAXH", sweep_time=None)
    b.ssa.write(":INIT")
    f = 7.1e6
    for i in range(30 if b.sim else 100):
        f += 1e3
        b.dut.freq(2, f)
        b.sleep(0.1)
    b.ssa.opc()
    fs, p = b.ssa.trace()
    p = p + b.pad_correction(10e6)
    b.s.save_data("L4.5_redraw", {"x": fs - 10e6, "level": p},
                  plot={"kind": "spectrum", "title": "CLK0 10 MHz while CH2 changes 10/s", "x": "x",
                        "series": ["level"], "xlabel": "Δf / Hz", "ylabel": "P / dBm"}, step="L4.5", mid="M-310")
    fpk, _ = analysis.peak(fs, p, 10e6, 50)
    _, sp = _carrier_and_spurs(fs, p, fpk, 200)
    top = sp[0] if sp else (float("nan"), float("nan"))
    b.info("L4.5", "M-310", "redraw: largest spur", top[1], "dBc", note="at {:+.0f} Hz".format(top[0]))


@step("L4.6", "Crosstalk CH2 (10.1 MHz) into CLK0", ["M-311"])
def l46(b, o):
    _std(b)
    _on(b, 1, 10e6)
    b.dut.freq(2, 10.1e6)
    for state in (False, True):
        b.dut.output(2, state)
        b.sleep(2)
        f, p = b.spectrum("L4.6_ch2_{}".format("on" if state else "off"), 10.05e6, 300e3, 100, "L4.6", "M-311",
                          "CLK0 10 MHz, CH2 10.1 MHz {}".format("on" if state else "off"), avg=5)
        _, carrier = analysis.peak(f, p, 10e6, 500)
        _, x = analysis.peak(f, p, 10.1e6 * (1 + b.cal.get("eps0_ppm", 0) * 0), 500)
        b.info("L4.6", "M-311", "line at 10.1 MHz, CH2 {}".format("on" if state else "off (MultiSynth runs)"),
               x - carrier, "dBc")


def _level(b, f):
    b.ssa.setup(f, 100e3, 1e3, ref_dbm=0.0)
    b.ssa.sweep()
    _, lv = b.ssa.marker_peak()
    return lv + b.pad_correction(f)


@step("L4.7", "Output level at 50 Ω", ["M-312"])
def l47(b, o):
    _std(b)
    _on(b, 1, 10e6)
    for ma in (2, 4, 6, 8):
        b.dut.drive(ma)
        b.sleep(1)
        b.info("L4.7", "M-312", "fundamental 10 MHz, {} mA".format(ma), _level(b, 10e6), "dBm")
    b.dut.drive(8)
    rows = {"f_MHz": [], "level_dBm": []}
    pts = sorted(set([1e6, 30e6, 100e6, 112.5e6, 150e6, 160e6] +
                     list(np.round(np.exp(np.linspace(math.log(1e6), math.log(160e6), 6 if o.quick else 20))))))
    for f in pts:
        b.dut.freq(1, f)
        b.sleep(1)
        lv = _level(b, f)
        rows["f_MHz"].append(f / 1e6)
        rows["level_dBm"].append(lv)
        if f in (1e6, 30e6, 100e6, 112.5e6, 150e6, 160e6):
            b.info("L4.7", "M-312", "fundamental {:.1f} MHz, 8 mA".format(f / 1e6), lv, "dBm")
    b.s.save_data("L4.7_level", rows, plot={"kind": "line", "title": "Fundamental at 50 Ω, drive 8 mA",
                                            "x": "f_MHz", "series": ["level_dBm"], "xscale": "log",
                                            "xlabel": "f / MHz", "ylabel": "P / dBm"}, step="L4.7", mid="M-312")


@step("L4.8", "Harmonics 2 ... 9 at 10 MHz per drive", ["M-414"])
def l48(b, o):
    _std(b)
    _on(b, 1, 10e6)
    att = b.cal.get("harm_atten", 30)
    cols = {"n": list(range(2, 10)), "ideal": [analysis.ideal_square_harmonic_dbc(n) for n in range(2, 10)]}
    cols["ideal"] = [v if v > -90.0 else float("nan") for v in cols["ideal"]]  # even: none
    for ma in (2, 4, 6, 8):
        b.dut.drive(ma)
        b.sleep(1)
        lv = harmonic_levels(b, 10e6, range(1, 10), atten=att)
        cols["{}mA".format(ma)] = [lv[n] - lv[1] for n in range(2, 10)]
        for n in range(2, 10):
            b.info("L4.8", "M-414", "H{} at {} mA".format(n, ma), lv[n] - lv[1], "dBc")
    b.s.save_data("L4.8_harmonics", cols, plot={"kind": "bar", "title": "Harmonics of CLK0 at 10 MHz",
                                                "x": "n", "series": ["ideal", "2mA", "4mA", "6mA", "8mA"],
                                                "xlabel": "harmonic n", "ylabel": "level / dBc"},
                  step="L4.8", mid="M-414")
    b.dut.drive(8)
    b.spectrum("L4.8_span", 50e6, 100e6, 10e3, "L4.8", "M-414", "CLK0 10 MHz, 8 mA, 0 ... 100 MHz",
               atten=att, ref_dbm=0.0)


@step("L4.9", "Phase noise at 10 and 14.2 MHz (upper bound)", ["M-416"])
def l49(b, o):
    _std(b)
    for f in (10e6, 14.2e6):
        _on(b, 1, f)
        b.sleep(2)
        for off in (100, 1e3, 10e3, 100e3):
            rbw = max(1.0, off / 30.0)
            rbw = 10 ** math.floor(math.log10(rbw))
            b.ssa.setup(f, max(4 * off, 1e3), rbw, ref_dbm=0.0, detector="AVER", trace_mode="AVER", average=10)
            b.ssa.sweep(count=10)
            fs, p = b.ssa.trace()
            fpk, _ = analysis.peak(fs, p, f, 3 * rbw)
            val = _noise_at(fs, p, fpk, off, rbw)
            own = {1e4: -98.0}.get(off)
            v = UNDECIDED if own is None or val < own + 6 else INFO
            b.s.record("L4.9", "M-416", "L({:.0f} Hz) at {:.1f} MHz".format(off, f / 1e6), val, "dBc/Hz",
                       verdict_=v, note="SSA own noise at 10 kHz: -98 dBc/Hz typ. (data sheet, 1 GHz)")


@step("L4.10", "Spread -0.5 %: band edges and modulation rate", ["M-422"])
def l410(b, o):
    _std(b)
    _on(b, 1, 10e6)
    b.dut.ssc("DOWN05")
    b.sleep(2)
    f, p = b.spectrum("L4.10_spread", 9.975e6, 100e3, 1e3, "L4.10", "M-422", "CLK0 10 MHz, spread -0.5 %",
                      maxhold_sweeps=20, offset_axis=False)
    lo, hi = analysis.band_edges(f, p, 10.0)
    ok = abs(lo - 9.95e6) < 3e3 and abs(hi - 10.0e6) < 3e3
    b.s.record("L4.10", "M-422", "band", "{:.4f} ... {:.4f}".format(lo / 1e6, hi / 1e6), "MHz",
               setpoint="9.95 ... 10.00", verdict_=PASS if ok else FAIL)
    b.ssa.setup(lo + 1e3, 0, 3e3, ref_dbm=0.0, detector="SAMP", sweep_time=1e-3)
    b.ssa.sweep()
    t, z = b.ssa.trace()
    try:
        rate = analysis.dominant_frequency(z, t[1] - t[0], f_min=5e3)
        b.info("L4.10", "M-422", "modulation rate (zero span at the band edge)", rate / 1e3, "kHz",
               setpoint=31.5)
    except Exception as exc:
        b.s.record("L4.10", "M-422", "modulation rate", "", "kHz", verdict_=UNDECIDED, note=str(exc))
    b.info("L4.10", "M-422", "FREQ:ACT? CH1 (mean)", b.dut.freq_actual(1), "Hz")
    b.dut.ssc("OFF")
    _on(b, 1, 10e6)
    b.spectrum("L4.10_nospread", 9.975e6, 100e3, 1e3, "L4.10", "M-422", "CLK0 10 MHz, no spread",
               maxhold_sweeps=5)


@step("L4.11", "455 kHz: harmonics up to 5 MHz", ["M-421"])
def l411(b, o):
    _std(b)
    _on(b, 1, 455e3)
    b.sleep(2)
    f, p = b.spectrum("L4.11_455k", 2.5e6, 5e6, 3e3, "L4.11", "M-421", "CLK0 455 kHz, 0 ... 5 MHz",
                      atten=b.cal.get("harm_atten", 30))
    for n, fn, lv, dbc in analysis.harmonics(f, p, 455e3, 10, 10e3):
        if n > 1:
            b.info("L4.11", "M-421", "H{} at 455 kHz".format(n), dbc, "dBc")


@step("L4.12", "WLAN on: spurs at the carrier", ["M-317", "M-318"])
def l412(b, o):
    _std(b)
    _on(b, 1, 10e6)
    b.sleep(2)
    _maxhold(b, "L4.12_wlan_off", "L4.12", "M-318", "CLK0 10 MHz, WLAN off", 60)
    b.confirm("Switch WLAN to Client (CONFIG Net or REST) and wait for the connection.")
    if b.sim:
        b.sim.dut.wlan = True
    f, p = _maxhold(b, "L4.12_wlan_on", "L4.12", "M-317", "CLK0 10 MHz, WLAN client", 60)
    fpk, _ = analysis.peak(f, p, 10e6, 50)
    _, sp = _carrier_and_spurs(f, p, fpk, 200)
    for off, dbc in sp[:3]:
        b.info("L4.12", "M-317", "WLAN on: spur", dbc, "dBc", note="at {:+.0f} Hz".format(off))
    b.confirm("Switch WLAN off again (standard condition).")
    if b.sim:
        b.sim.dut.wlan = False


# =========================================================================== L5: warm-up


@step("L5.1", "Warm-up drift over 60 min and Allan deviation", ["M-313", "M-411"], "L5")
def l51(b, o):
    b.plug("cold")
    b.dut.standard(b.cal.get("corr"))
    for ch in (1, 2):
        _on(b, ch, 10e6)
    temp0 = b.temperature()
    t0 = b.now()
    rows = {"t_min": [], "sds_ppm": [], "ssa_ppm": []}
    b.scope_frequency_setup(10e6)
    n = int(o.warmup_minutes * 6)
    for i in range(n + 1):
        f, _ = b.measure_frequency(10e6, setup=False)
        rows["t_min"].append((b.now() - t0) / 60.0)
        rows["sds_ppm"].append(analysis.ppm(f, 10e6))
        if i % 6 == 0:
            rows["ssa_ppm"].append(analysis.ppm(b.counter(10e6), 10e6))
            b.scope_frequency_setup(10e6)
        else:
            rows["ssa_ppm"].append(float("nan"))
        b.sleep(10)
    temp1 = b.temperature()
    t = np.array(rows["t_min"])
    y = np.array(rows["sds_ppm"])

    def at(m):
        return float(np.interp(m, t, y))

    b.info("L5.1", "M-313", "drift 0 ... 15 min", at(15) - at(0), "ppm")
    b.info("L5.1", "M-313", "drift 15 ... {} min".format(o.warmup_minutes), at(o.warmup_minutes) - at(15), "ppm")
    settle = None
    for m in np.arange(0, max(o.warmup_minutes - 10, 0) + 1):
        if abs(at(m + 10) - at(m)) < 0.1:
            settle = float(m)
            break
    b.info("L5.1", "M-313", "time until drift < 0.1 ppm / 10 min", settle if settle is not None else "> run",
           "min", note="room temperature {} -> {} °C".format(temp0, temp1))
    b.s.save_data("L5.1_warmup", rows, plot={"kind": "line", "title": "Frequency deviation after power-on (10 MHz)",
                                             "x": "t_min", "series": ["sds_ppm", "ssa_ppm"], "xlabel": "t / min",
                                             "ylabel": "Δf/f / ppm"}, step="L5.1", mid="M-313")
    # M-411: 100 values as fast as the transfer allows
    ts, ys = [], []
    for _ in range(30 if o.quick else 100):
        f, _ = b.measure_frequency(10e6, setup=False)
        ts.append(b.now())
        ys.append((f - 10e6) / 10e6)
        b.sleep(1.0)
    tau0 = float(np.median(np.diff(ts))) if len(ts) > 1 else 1.0
    ad = analysis.allan_deviation(np.array(ys) - np.mean(ys), tau0)
    for tau, dev, nn in ad:
        b.info("L5.1", "M-411", "Allan deviation τ = {:.0f} s".format(tau), dev, "", note="{} terms".format(nn))
    if ad:
        b.s.save_data("L5.1_allan", {"tau_s": [a[0] for a in ad], "adev": [a[1] for a in ad]},
                      plot={"kind": "line", "title": "Allan deviation at 10 MHz", "x": "tau_s", "series": ["adev"],
                            "xscale": "log", "yscale": "log", "xlabel": "τ / s", "ylabel": "σ_y(τ)"},
                      step="L5.1", mid="M-411")
    b.plug("G")


# =========================================================================== L6: variants


@step("L6.1", "S1: continuous tuning of CH1 on the SDS", ["M-114"])
def l61(b, o):
    b.plug("S1")
    _continuous_tuning(b, 1, "L6.1")
    b.plug("G")


@step("L6.2", "S1: crosstalk CH1 (10.1 MHz) into CLK1", ["M-311"])
def l62(b, o):
    b.plug("S1")
    _std(b)
    _on(b, 2, 10e6)
    b.dut.freq(1, 10.1e6)
    for state in (False, True):
        b.dut.output(1, state)
        b.sleep(2)
        f, p = b.spectrum("L6.2_ch1_{}".format("on" if state else "off"), 10.05e6, 300e3, 100, "L6.2", "M-311",
                          "CLK1 10 MHz, CH1 10.1 MHz {}".format("on" if state else "off"), avg=5)
        _, carrier = analysis.peak(f, p, 10e6, 500)
        _, x = analysis.peak(f, p, 10.1e6, 500)
        b.info("L6.2", "M-311", "line at 10.1 MHz in CLK1, CH1 {}".format("on" if state else "off"), x - carrier, "dBc")
    b.plug("G")


@step("L6.3", "Spectrum of CLK2 in IQ mode", ["M-417"])
def l63(b, o):
    b.plug("CLK2-SSA")
    for f in (4.73e6, 10e6):
        _iq(b, f, master=2)
        b.dut.write("SOUR3:PHAS 90")
        b.sleep(2)
        fs, p = b.spectrum("L6.3_clk2_{:.2f}MHz".format(f / 1e6), f, 200e3, 100, "L6.3", "M-417",
                           "CLK2 (IQ 90°) {:.2f} MHz".format(f / 1e6), avg=10, offset_axis=True)
        fpk, _ = analysis.peak(fs, p, f, 2e3)
        _, sp = _carrier_and_spurs(fs, p, fpk, 1e3)
        top = sp[0] if sp else (float("nan"), float("nan"))
        b.info("L6.3", "M-417", "CLK2 {:.2f} MHz: largest spur".format(f / 1e6), top[1], "dBc",
               note="at {:+.0f} Hz; compare with CLK0 in L4".format(top[0]))
    b.plug("G")


@step("L6.4", "Levels in the OFF state per Off state (S1 + S2)", ["M-117", "M-118"])
def l64(b, o):
    rows = {"case": [], "clk0_V": [], "clk2_V": []}
    for term, plug in (("1M", ["S1", "S2"]), ("50", ["S1"])):
        b.plug(plug)
        vs = V50 if term == "50" else V1M
        for mode in ("LOW", "HIGH", "HIZ"):
            _std(b)
            b.dut.offstate(mode)
            _on(b, 1, 10e6)
            b.dut.iq(1, 0)
            b.dut.output(3, True)
            b.sleep(1)
            b.dut.output(3, False)
            b.dut.output(1, False)
            b.sleep(1)
            b.sds.only(SIG_CH, IQ_CH, all=vs)
            b.sds.timebase(1e-6)
            b.sds.trigger_edge(SIG_CH, 5.0)
            levels = {}
            for ch in (SIG_CH, IQ_CH):
                levels[ch] = b.sds.measure(ch, "MEAN")
            rows["case"].append("{} {}".format(mode, term))
            rows["clk0_V"].append(levels[SIG_CH])
            rows["clk2_V"].append(levels[IQ_CH])
            for ch, name in ((SIG_CH, "CLK0"), (IQ_CH, "CLK2")):
                v = levels[ch]
                if mode == "LOW":
                    vd = PASS if abs(v) < 0.1 else FAIL
                elif mode == "HIGH" and term == "1M":
                    vd = PASS if v > 3.0 else FAIL
                else:
                    vd = INFO
                mid = "M-118" if name == "CLK2" and mode == "LOW" else "M-117"
                b.s.record("L6.4", mid, "{} off, Off state {}, {} Ω".format(name, mode, term), v, "V", verdict_=vd)
    b.dut.offstate("LOW")
    b.s.save_data("L6.4_offstate", rows, plot={"kind": "bar", "title": "Level of switched-off outputs",
                                               "x": "case", "series": ["clk0_V", "clk2_V"], "xlabel": "",
                                               "ylabel": "U / V"}, step="L6.4", mid="M-117")
    b.manual("L6.4", "M-117", "DMM at CLK0, CLK1, CLK2; High-Z with 10 kΩ to 3.3 V / GND", "MESSPLAN M-117 step 2")
    b.manual("L6.4", "M-118", "CLK2 with PDN: 10 kΩ to 3.3 V / GND (pulls low or high-Z?)", "MESSPLAN M-118 step 2")
    b.plug("G")


@step("L6.5", "Output at restart (EN on SDS channel 2)", ["M-119"])
def l65(b, o):
    b.plug("S3a")
    durs = []
    keep = None
    for i in range(3 if o.quick else o.resets):
        _std(b)
        _on(b, 2, 10e6)
        b.sleep(1)
        b.sds.only(1, 2, per={1: V50, 2: {"vdiv": 1.0, "offset": -1.65}})
        b.sds.memory("1.4M")
        b.sds.timebase(0.1)
        b.sds.trigger_edge(2, 1.6, "NEG")
        b.sds.arm_single()
        b.sleep(0.2)
        b.dut.pulse_reset()
        b.sleep(3)
        data, dt = {c: b.sds.waveform(c)[0] for c in (1, 2)}, b.sds.waveform(1)[1]
        try:
            t_en = analysis.falling_edge_time(data[2], dt)
        except analysis.AnalysisError:
            b.s.record("L6.5", "M-119", "restart {}: no EN edge".format(i + 1), "", "", verdict_=UNDECIDED)
            continue
        end = analysis.envelope_end(data[1], dt, 1e-3)
        d = (end - t_en) if end is not None else float("nan")
        durs.append(d)
        if keep is None:
            keep = (data, dt)
        b.sleep(3)
    if durs:
        b.info("L6.5", "M-119", "EN low until the end of the old signal: min / max",
               "{:.0f} / {:.0f}".format(min(durs) * 1e3, max(durs) * 1e3), "ms", note="{} restarts".format(len(durs)))
    if keep:
        data, dt = keep
        k = max(1, data[1].size // 4000)
        t = np.arange(data[1].size)[::k] * dt * 1e3
        b.s.save_data("L6.5_reset", {"t_ms": t, "clk1": data[1][::k], "en": data[2][::k]},
                      plot={"kind": "wave", "title": "Restart: EN and CLK1", "x": "t_ms",
                            "subplots": [["en"], ["clk1"]], "subtitles": ["EN", "CLK1 (10 MHz, undersampled)"],
                            "xlabel": "t / ms", "ylabel": "U / V"}, step="L6.5", mid="M-119")
    b.manual("L6.5", "M-119", "once 'pio run -t upload': time until the old signal ends", "trigger on EN, read off")
    b.plug("G")


@step("L6.6", "I2C with the SDS decoder (S3b)", ["M-314", "M-315"])
def l66(b, o):
    b.plug("S3b")
    b.confirm("Set the I2C decoder (SDA = C2, SCL = C4) and the I2C trigger on address 0x60 at the SDS by hand.")
    b.info("L6.6", "M-315", "SYST:SI5351?", b.dut.query("SYST:SI5351?"), "")
    b.screenshot("sds", "L6.6_i2c")
    b.manual("L6.6", "M-314", "SDA to GND: code 5, dialog 'I2C line stays low', GUI stall < 100 ms",
             "MESSPLAN M-314")
    b.manual("L6.6", "M-315", "register dump in the log, no 'Unexpected reg183'", "MESSPLAN M-315")
    b.plug("G")


@step("L6.7", "Supply variants", ["M-310"])
def l67(b, o):
    for name in ("pc-usb", "usb-supply", "powerbank"):
        b.confirm("Supply: {} (without USB data: set CH1 = 10 MHz on at the device by hand).".format(name))
        try:
            _std(b)
            _on(b, 1, 10e6)
        except Exception as exc:
            print("  no SCPI with this supply: {}".format(exc))
        b.sleep(2)
        f, p = _maxhold(b, "L6.7_supply_{}".format(name), "L6.7", "M-310", "CLK0 10 MHz, supply {}".format(name), 60)
        fpk, _ = analysis.peak(f, p, 10e6, 50)
        _, sp = _carrier_and_spurs(f, p, fpk, 200)
        for off, dbc in sp[:2]:
            b.info("L6.7", "M-310", "{}: spur".format(name), dbc, "dBc", note="at {:+.0f} Hz".format(off))
    b.confirm("Back to the standard supply (USB isolator) and reconnect.")


@step("L6.8", "IQ error state with the faultiq build", ["M-401", "M-402"])
def l68(b, o):
    b.confirm("Flash the test build: pio run -e esp32-2432S028R-faultiq -t upload --upload-port <port>")
    if b.sim:
        b.sim.dut.faultiq = True
    b.plug(["S2"])
    _std(b)
    _on(b, 2, 10e6)
    b.dut.iq(2, 270)
    b.dut.output(3, True)
    b.sleep(3)
    cond = b.dut.condition(3)
    b.s.record("L6.8", "M-401", "OUTP3:COND? in the error state", cond, "", verdict_=PASS if cond == "ERR" else FAIL)
    for mode in ("HIGH", "LOW", "HIZ"):
        b.dut.offstate(mode)
        b.sleep(60)
        b.sds.only(IQ_CH, all=V1M)
        b.sds.timebase(1e-6)
        pp = b.sds.measure(IQ_CH, "PKPK")
        b.s.record("L6.8", "M-402", "CLK2 in ERR, Off state {}: peak-peak".format(mode), pp, "V", limit=0.3,
                   verdict_=PASS if pp < 0.3 else FAIL, note="must not oscillate")
    b.dut.offstate("LOW")
    b.manual("L6.8", "M-401", "status on MAIN, header and CH3 page; one dialog with channel and remedy",
             "MESSPLAN M-401")
    b.plug("G")
    b.confirm("Flash the release build again (pio run -e esp32-2432S028R -t upload).")
    if b.sim:
        b.sim.dut.faultiq = False


GROUPS = collections.OrderedDict([
    ("V", ["V-01", "V-02", "V-03", "V-04", "V-04b", "V-05", "V-06", "V-07"]),
    ("L1", [k for k in STEPS if k.startswith("L1.")]),
    ("L2", [k for k in STEPS if k.startswith("L2.")]),
    ("L3", [k for k in STEPS if k.startswith("L3.")]),
    ("L4", [k for k in STEPS if k.startswith("L4.")]),
    ("L5", [k for k in STEPS if k.startswith("L5.")]),
    ("L6", [k for k in STEPS if k.startswith("L6.")]),
])


def resolve(names):
    """Step IDs from group names (V, L1 ... L6, all = V + L1 ... L4 + L6) and single IDs."""
    out = []
    for n in names:
        if n == "all":
            for g in ("V", "L1", "L2", "L3", "L4", "L6"):
                out += GROUPS[g]
        elif n in GROUPS:
            out += GROUPS[n]
        elif n in STEPS:
            out.append(n)
        else:
            raise KeyError("unknown step or group '{}'".format(n))
    seen = set()
    return [x for x in out if not (x in seen or seen.add(x))]
