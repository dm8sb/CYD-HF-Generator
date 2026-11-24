#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Tests of the measurement scripts (MESSPLAN_SIGLENT.md 8): evaluation against synthetic data
with known frequency, phase and duty cycle, then an end-to-end dry run on the simulated bench up
to the protocol.

    python tools/messung/test_messung.py          (needs numpy and matplotlib)
"""
import os
import shutil
import sys
import tempfile
import unittest

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import analysis  # noqa: E402
import store  # noqa: E402


def square(f, fs, n, phase_deg=0.0, duty=0.5, vpp=1.0, rise=1.75e-9, noise=0.003, tb_ppm=0.0,
           bits=8, vdiv=0.25, offset=-0.75, seed=0, t0=0.0):
    """Band-limited CMOS-like square wave as an 8-bit oscilloscope would record it."""
    rng = np.random.default_rng(seed)
    t = t0 + np.arange(n) / (fs * (1 + tb_ppm * 1e-6))
    p = (f * t - phase_deg / 360.0) % 1.0
    T = 1.0 / f
    sigma = rise / 2.5
    inside = p < duty
    s = np.where(inside, np.minimum(p, duty - p), -np.minimum(p - duty, 1.0 - p)) * T
    y = vpp * 0.5 * (1 + np.tanh(0.7978845608 * (s / sigma + 0.044715 * (s / sigma) ** 3)))
    y = y + rng.normal(0, noise, n)
    if bits:
        step = vdiv / 25.0
        y = np.clip(np.round((y + offset) / step), -128, 127) * step - offset
    return y


def sine(f, fs, n, vpp=1.6, noise=0.003, tb_ppm=0.0, seed=1):
    rng = np.random.default_rng(seed)
    t = np.arange(n) / (fs * (1 + tb_ppm * 1e-6))
    y = vpp / 2 * np.sin(2 * np.pi * f * t) + rng.normal(0, noise, n)
    step = 0.25 / 25.0
    return np.clip(np.round(y / step), -128, 127) * step


class TestEdgesAndFrequency(unittest.TestCase):
    FS = 1e9
    N = 400000

    def test_ratio_cancels_time_base(self):
        """2.1: a 25 ppm time base error must not show up in the ratio to the reference."""
        for f in (7.1e6, 10e6, 28.4e6, 99.999999e6, 160e6):
            sig = square(f, self.FS, self.N, tb_ppm=25.0, seed=2)
            ref = sine(10e6, self.FS, self.N, tb_ppm=25.0, seed=3)
            fm, u = analysis.frequency_ratio(sig, ref, 1 / self.FS)
            self.assertLess(abs(fm - f) / f, 5e-8, f)
            self.assertLess(abs(fm - f), 4 * u + 1e-3, f)

    def test_frequency_direct_sees_time_base(self):
        sig = square(10e6, self.FS, self.N, tb_ppm=25.0)
        fm, _ = analysis.frequency(sig, 1 / self.FS)
        self.assertAlmostEqual((fm - 10e6) / 10e6 * 1e6, -25.0, delta=0.1)

    def test_long_record_numbering(self):
        """Regression: the edge numbering must not slip over many periods (median period error)."""
        sig = square(28.4e6, self.FS, 1000000, seed=4)
        fm, _ = analysis.frequency(sig, 1 / self.FS)
        self.assertLess(abs(fm - 28.4e6) / 28.4e6, 1e-7)

    def test_missing_edges_counted(self):
        sig = square(10e6, self.FS, 200000)
        sig[50000:50300] = sig[49990]  # 3 periods without edges
        fm, _ = analysis.frequency(sig, 1 / self.FS)
        self.assertLess(abs(fm - 10e6) / 10e6, 1e-6)

    def test_too_few_edges(self):
        with self.assertRaises(analysis.AnalysisError):
            analysis.frequency(np.zeros(1000), 1e-9)


class TestPhaseDutyDropouts(unittest.TestCase):
    FS = 1e9
    N = 200000

    def test_phase_all_quadrants(self):
        for f in (4.73e6, 10e6, 99e6):
            a = square(f, self.FS, self.N, 0.0, seed=5)
            for ph in (0.0, 90.0, 180.0, 270.0, 359.5):
                b = square(f, self.FS, self.N, ph, seed=6)
                r = analysis.phase(a, b, 1 / self.FS, f)
                err = analysis.phase_error(r["phase"], ph)
                self.assertLess(abs(err), 0.3, (f, ph, r["phase"]))

    def test_phase_from_time_offset(self):
        """30 ps skew at 99 MHz is 1.07 degrees; at 10 MHz every edge falls at the same place between
        two samples (coherent), where plain linear interpolation read 50 ps instead of 30 ps."""
        for f, tol_ps in ((99e6, 3.0), (98.7654e6, 3.0), (10e6, 5.0)):
            a = square(f, self.FS, self.N, 0.0, seed=7)
            b = square(f, self.FS, self.N, 30e-12 * f * 360.0, seed=8)  # b 30 ps later
            r = analysis.phase(a, b, 1 / self.FS, f)
            dt_ps = analysis.phase_error(r["phase"], 0.0) / 360.0 / f * 1e12
            self.assertAlmostEqual(dt_ps, 30.0, delta=tol_ps, msg=str(f))

    def test_duty_cycle(self):
        for duty in (0.48, 0.5, 0.52):
            y = square(10e6, self.FS, self.N, duty=duty)
            self.assertAlmostEqual(analysis.duty_cycle(y, 1 / self.FS), duty * 100, delta=0.3)

    def test_dropouts_and_gaps(self):
        y = square(10e6, self.FS, self.N)
        y[100000:103000] = y[99990]
        d = analysis.dropouts(y, 1 / self.FS, 10e6)
        self.assertEqual(len(d), 1)
        self.assertAlmostEqual(d[0][1], 3.0e-6, delta=0.2e-6)
        g = analysis.gaps(y, 1 / self.FS, 3 / 10e6)
        self.assertEqual(len(g), 1)

    def test_envelope_end(self):
        fs = 1e6
        t = np.arange(1400000) / fs
        # 10 MHz sampled at 1 MSa/s: the samples alias, but the record toggles while active
        y = np.where((t < 0.47), (np.sin(2 * np.pi * 10.000337e6 * t + 0.3) > 0).astype(float), 0.0)
        self.assertAlmostEqual(analysis.envelope_end(y, 1 / fs, 1e-3), 0.47, delta=2e-3)

    def test_rise_time(self):
        y = square(10e6, 10e9, 100000, rise=2e-9, noise=0.0005, bits=0)
        self.assertAlmostEqual(analysis.rise_time(y, 1e-10), 2e-9, delta=0.4e-9)


class TestHelpers(unittest.TestCase):
    def test_bracket(self):
        # +1 ppm reference drift is removed
        self.assertAlmostEqual(analysis.bracket_delta(100e6 * (1 + 1e-6) + 0.3, 100e6, 10e6 * (1 + 1e-6),
                                                      10e6 * (1 + 1e-6)), 0.3, places=6)

    def test_allan_white_frequency(self):
        rng = np.random.default_rng(9)
        y = rng.normal(0, 1e-9, 4000)
        ad = dict((round(t), a) for t, a, _ in analysis.allan_deviation(y, 1.0))
        self.assertAlmostEqual(ad[1] / 1e-9, 1.0, delta=0.1)
        self.assertAlmostEqual(ad[100] / ad[1], 0.1, delta=0.05)

    def test_ideal_square(self):
        self.assertAlmostEqual(analysis.ideal_square_harmonic_dbc(3), -9.54, places=2)
        self.assertAlmostEqual(analysis.ideal_square_harmonic_dbc(5), -13.98, places=2)
        self.assertEqual(analysis.ideal_square_harmonic_dbc(2), float("-inf"))
        self.assertAlmostEqual(analysis.ideal_square_harmonic_dbc(2, 0.49), -30.1, delta=0.2)

    def test_spurs_and_harmonics(self):
        f = np.linspace(9.9e6, 10.1e6, 2001)
        p = np.full(f.size, -100.0)
        p[1000] = 0.0
        p[1183] = -70.0
        c, sp = analysis.spurs(f, p, 10e6, 1e3)
        self.assertEqual(c, 0.0)
        self.assertAlmostEqual(sp[0][0], 18.3e3, delta=100)
        self.assertAlmostEqual(sp[0][1], -70.0)

    def test_verdict(self):
        self.assertEqual(store.verdict(0.3, 0.1, 0.5), store.PASS)
        self.assertEqual(store.verdict(0.7, 0.1, 0.5), store.FAIL)
        self.assertEqual(store.verdict(0.45, 0.1, 0.5), store.UNDECIDED)
        self.assertEqual(store.verdict(1.0, 0.0, None), store.INFO)


class TestDryRun(unittest.TestCase):
    """The scripts on the simulated bench, from the first step to the protocol."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_run_and_report(self):
        import measure

        rc = measure.main(["run", "V-01", "V-02", "V-03", "V-04", "V-07", "L1.1", "L1.2", "L1.3", "L2.6", "L3.1",
                           "L4.1", "L6.5", "L6.8", "--sim", "--quick", "--out", self.tmp, "--name", "t"])
        self.assertEqual(rc, 0)
        proto = os.path.join(self.tmp, "t.md")
        with open(proto, encoding="utf-8") as fh:
            text = fh.read()
        for needle in ("M-107", "M-108", "M-115", "M-301", "M-119", "M-401", "L3.1_iq_waves.png",
                       "<!-- hand:findings -->"):
            self.assertIn(needle, text)
        rows = store.Session(self.tmp, "t").results()
        by = {(r["step"], r["quantity"]): r for r in rows}
        crystal = float(by[("L1.1", "crystal error")]["value"])
        self.assertAlmostEqual(crystal, 9.7, delta=0.5)  # simulated crystal error
        self.assertEqual(by[("L1.2", "deviation at 10 MHz after calibration")]["verdict"], store.PASS)
        self.assertEqual(by[("L6.8", "OUTP3:COND? in the error state")]["verdict"], store.PASS)
        self.assertTrue(os.path.exists(os.path.join(self.tmp, "t", "img", "L3.1_iq_waves.png")))
        # hand blocks survive a new generation
        text = text.replace("Befunde MB-n (Mess-ID, Messwert, Bild, Vermutung, Aufgabe in ARBEITSPLAN.md):\n",
                            "MB-1: test entry\n")
        with open(proto, "w", encoding="utf-8") as fh:
            fh.write(text)
        import report

        report.generate(os.path.join(self.tmp, "t"))
        with open(proto, encoding="utf-8") as fh:
            self.assertIn("MB-1: test entry", fh.read())

    def test_resolve(self):
        import steps

        ids = steps.resolve(["all"])
        self.assertEqual(ids[0], "V-01")
        self.assertNotIn("L5.1", ids)
        self.assertIn("L6.8", ids)
        self.assertEqual(steps.resolve(["L1.1", "L1"])[:2], ["L1.1", "L1.2"])
        with self.assertRaises(KeyError):
            steps.resolve(["X9"])


if __name__ == "__main__":
    unittest.main(verbosity=1)
