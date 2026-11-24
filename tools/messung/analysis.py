# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Evaluation of the automated measurements (MESSPLAN_SIGLENT.md section 2), without instruments.

Everything here works on plain numpy arrays, so it is tested against synthetic data
(test_messung.py) before it judges a real device:

- edge times with hysteresis and linear interpolation (2.1, 2.3)
- frequency of a channel as ratio to the GPS reference in the same record (2.1): the
  oscilloscope time base (+-25 ppm) cancels
- phase and time offset between two channels (2.3), duty cycle, dropouts (2.4)
- bracket measurement (MESSPLAN.md 3.3), Allan deviation (M-411)
- spectrum helpers: carrier, spurs, harmonics (2.5)
"""
import math

import numpy as np


class AnalysisError(ValueError):
    """The record does not allow the evaluation (too few edges, no signal)."""


# --------------------------------------------------------------------------- edges


def levels(y):
    """Low and high level of a two-level signal (1st and 99th percentile)."""
    lo, hi = np.percentile(y, [1.0, 99.0])
    return float(lo), float(hi)


LANCZOS_A = 8  # half width of the interpolation kernel in samples


def _lanczos_value(y, t):
    """Band-limited (Lanczos-windowed sinc) interpolation of y at fractional indices t."""
    a = LANCZOS_A
    k0 = np.floor(t).astype(np.int64)
    offs = np.arange(-a + 1, a + 1)
    idx = k0[:, None] + offs[None, :]
    x = t[:, None] - idx
    w = np.sinc(x) * np.sinc(x / a)
    return np.sum(y[np.clip(idx, 0, y.size - 1)] * w, axis=1)


def _refine(y, t, level, iterations=4):
    """Newton steps on the interpolated signal: t where y(t) = level (fractional indices).

    Linear interpolation between two samples has an error that depends on where the edge falls
    between the samples. When the signal frequency is a simple fraction of the sample rate
    (10 MHz at 1 GSa/s), every edge falls at the same place and the error does not average out:
    30 ps between two channels read as 50 ps. The oscilloscope bandwidth (200 MHz) is well below
    half the sample rate, so the sinc interpolation reconstructs the edge between the samples.
    """
    a = LANCZOS_A
    ok = (t > a) & (t < y.size - a - 1)
    tt = t[ok].copy()
    h = 0.05
    for _ in range(iterations):
        v = _lanczos_value(y, tt) - level
        dv = (_lanczos_value(y, tt + h) - _lanczos_value(y, tt - h)) / (2 * h)
        good = np.abs(dv) > 1e-12
        stepv = np.zeros_like(tt)
        stepv[good] = v[good] / dv[good]
        tt = tt - np.clip(stepv, -0.5, 0.5)
    out = t.copy()
    out[ok] = tt
    return out


MAX_REFINED = 20000  # edges refined per evaluation; more add little and cost seconds


def edges(y, dt=1.0, rising=True, hysteresis=0.2, refine=True):
    """Times of the 50 % crossings of a two-level signal (all edges, units of dt).

    A crossing counts only when the signal goes from below the lower to above the upper
    hysteresis threshold (mid -+ hysteresis * swing / 2), so noise on the 50 % level does not
    double an edge. The time comes from linear interpolation between the two samples around the
    50 % level, refined by sinc interpolation (refine=True, see _refine).
    """
    t, ys, mid = _crossings(y, rising, hysteresis)
    if refine:
        t = _refine(ys, t, mid)
    return t * dt


def _crossings(y, rising=True, hysteresis=0.2):
    """Linear crossing times in samples, the (possibly inverted) signal and the 50 % level."""
    y = np.asarray(y, dtype=float)
    if y.size < 4:
        raise AnalysisError("record too short")
    lo, hi = levels(y)
    swing = hi - lo
    if swing <= 0:
        raise AnalysisError("no signal (flat record)")
    if not rising:
        y = -y
        lo, hi = -hi, -lo
    mid = (lo + hi) / 2.0
    th_lo = mid - hysteresis * swing / 2.0
    th_hi = mid + hysteresis * swing / 2.0
    # state: 0 below th_lo, 1 above th_hi, carried forward in between
    state = np.full(y.size, -1, dtype=np.int8)
    state[y < th_lo] = 0
    state[y > th_hi] = 1
    idx = np.where(state >= 0)[0]
    if idx.size < 2:
        raise AnalysisError("no transitions")
    st = state[idx]
    # transitions 0 -> 1 between consecutive defined samples
    up = np.where((st[:-1] == 0) & (st[1:] == 1))[0]
    k_after = idx[up + 1]  # first sample above th_hi
    # all 50 % crossings (upwards) as candidates
    cand = np.where((y[:-1] < mid) & (y[1:] >= mid))[0]
    if cand.size == 0 or k_after.size == 0:
        raise AnalysisError("no edges")
    pos = np.searchsorted(cand, k_after, side="left") - 1
    ok = pos >= 0
    c = cand[pos[ok]]
    c = np.unique(c)
    y0 = y[c]
    y1 = y[c + 1]
    frac = (mid - y0) / (y1 - y0)
    return c + frac, y, mid


def number_edges(t):
    """Period number of every edge from the rounded spacing to its predecessor.

    A small error of the median period cannot add up over thousands of periods, and a missing
    edge (dropout) still counts as k periods.
    """
    t = np.asarray(t, dtype=float)
    d = np.diff(t)
    p0 = float(np.median(d))
    return np.concatenate(([0.0], np.cumsum(np.maximum(np.round(d / p0), 1.0))))


def _subset(count, limit=None):
    limit = MAX_REFINED if limit is None else limit
    if count <= limit:
        return np.arange(count)
    return np.unique(np.linspace(0, count - 1, limit).astype(np.int64))


def refined_edges(y, dt, rising=True, limit=None):
    """Up to `limit` edges spread evenly over the record, refined; with their period numbers.

    Returns (times in units of dt, period numbers). The numbers come from all edges, so the
    subset still describes the whole record.
    """
    t, ys, mid = _crossings(y, rising)
    if t.size < 8:
        raise AnalysisError("too few edges ({})".format(t.size))
    n = number_edges(t)
    k = _subset(t.size, limit)
    return _refine(ys, t[k], mid) * dt, n[k]


def period_fit(times, numbers=None):
    """Period by a straight line through edge time over period number.

    Returns (period, standard error of the period, residual rms).
    """
    t = np.asarray(times, dtype=float)
    if t.size < 8:
        raise AnalysisError("too few edges ({})".format(t.size))
    n = number_edges(t) if numbers is None else np.asarray(numbers, dtype=float)
    n_mean = n.mean()
    t_mean = t.mean()
    sxx = float(np.sum((n - n_mean) ** 2))
    period = float(np.sum((n - n_mean) * (t - t_mean)) / sxx)
    resid = t - (t_mean + period * (n - n_mean))
    dof = max(t.size - 2, 1)
    s = math.sqrt(float(np.sum(resid ** 2)) / dof)
    return period, s / math.sqrt(sxx), s


def frequency(y, dt):
    """Frequency of one channel in units of the record's time base: (f, u_f)."""
    t, n = refined_edges(y, dt)
    period, u_period, _ = period_fit(t, n)
    f = 1.0 / period
    return f, f * u_period / period


def frequency_ratio(y_sig, y_ref, dt=1.0, f_ref=10e6):
    """Frequency of y_sig from the ratio to the reference y_ref in the same record (2.1).

    The common time base cancels: f = f_ref * f_sig / f_ref_measured. Returns (f, u_f) in Hz,
    u_f from the standard errors of both line fits.
    """
    fs, us = frequency(y_sig, dt)
    fr, ur = frequency(y_ref, dt)
    f = f_ref * fs / fr
    u = f * math.hypot(us / fs, ur / fr)
    return f, u


# --------------------------------------------------------------------------- phase, duty, dropouts


def phase(y_ref, y_sig, dt, f=None):
    """Phase of y_sig behind y_ref in degrees (0 ... 360) from the rising edges (2.3).

    Every edge of y_ref is paired with the next edge of y_sig; the delay modulo the period is
    averaged as an angle (circular mean), so values around 0/360 degrees do not split.
    Returns dict(phase, dt_mean, u_phase, std_phase, period, n).
    """
    ta_lin, ya, mida = _crossings(y_ref)
    tb_lin, yb, midb = _crossings(y_sig)
    if f is None:
        period = period_fit(ta_lin * dt)[0]
    else:
        period = 1.0 / f
    k = _subset(ta_lin.size)
    j = np.searchsorted(tb_lin, ta_lin[k] - 1.0)  # the edge of y_sig at or after the y_ref edge
    ok = j < tb_lin.size
    ta = _refine(ya, ta_lin[k][ok], mida) * dt
    tb = _refine(yb, tb_lin[j[ok]], midb) * dt
    d = tb - ta
    if d.size < 4:
        raise AnalysisError("too few edge pairs")
    ang = 2.0 * np.pi * (d / period)
    z = np.exp(1j * ang)
    zm = z.mean()
    mean = (math.degrees(math.atan2(zm.imag, zm.real)) + 360.0) % 360.0
    dev = np.angle(z * np.conj(zm), deg=True)
    std = float(np.std(dev))
    return {
        "phase": mean,
        "dt_mean": mean / 360.0 * period,
        "std_phase": std,
        "u_phase": std / math.sqrt(d.size),
        "period": period,
        "n": int(d.size),
    }


def phase_error(measured, nominal):
    """Signed difference measured - nominal in degrees, wrapped to -180 ... 180."""
    return (measured - nominal + 180.0) % 360.0 - 180.0


def duty_cycle(y, dt):
    """Duty cycle in percent: mean high time over period at the 50 % level."""
    tr_lin, yr, midr = _crossings(y, True)
    tf_lin, yf, midf = _crossings(y, False)
    period = period_fit(tr_lin * dt)[0]
    k = _subset(tr_lin.size)
    j = np.searchsorted(tf_lin, tr_lin[k])
    ok = j < tf_lin.size
    tr = _refine(yr, tr_lin[k][ok], midr) * dt
    tf = _refine(yf, tf_lin[j[ok]], midf) * dt
    high = tf - tr
    high = high[(high > 0) & (high < period)]
    if high.size < 4:
        raise AnalysisError("too few periods for the duty cycle")
    return float(np.mean(high) / period * 100.0)


def dropouts(y, dt, f, factor=1.5):
    """Gaps in the edge sequence longer than factor periods: list of (start, duration) in s."""
    t = edges(y, dt, refine=False)
    gaps = np.diff(t)
    k = np.where(gaps > factor / f)[0]
    return [(float(t[i]), float(gaps[i])) for i in k]


def gaps(y, dt, min_s):
    """Intervals without any transition longer than min_s: list of (start, duration) in s.

    Works on undersampled records too (both edge directions count), so it finds dropouts of a
    100 MHz signal in a record with 100 MSa/s.
    """
    try:
        t = np.sort(np.concatenate((edges(y, dt, True, refine=False), edges(y, dt, False, refine=False))))
    except AnalysisError:
        return [(0.0, len(y) * dt)]
    bounds = np.concatenate(([0.0], t, [len(y) * dt]))
    d = np.diff(bounds)
    k = np.where(d > min_s)[0]
    return [(float(bounds[i]), float(d[i])) for i in k]


def envelope_end(y, dt, block_s, threshold=None):
    """End of activity: last time (s) where the peak-to-peak value within a block exceeds the
    threshold (default half of the overall swing). For the restart capture (M-119), where a
    10 MHz output is sampled far below its frequency."""
    n = max(1, int(round(block_s / dt)))
    m = len(y) // n
    if m == 0:
        return None
    blocks = np.asarray(y[:m * n], dtype=float).reshape(m, n)
    pp = blocks.max(axis=1) - blocks.min(axis=1)
    if threshold is None:
        threshold = 0.5 * float(np.ptp(y))
    act = np.where(pp > threshold)[0]
    return float((act[-1] + 1) * n * dt) if act.size else None


def falling_edge_time(y, dt):
    """Time (s) of the first falling 50 % crossing (EN line in M-119)."""
    t = edges(y, dt, rising=False, refine=False)
    return float(t[0])


def rise_time(y, dt, lo=0.1, hi=0.9):
    """Mean 10-90 % rise time of the rising edges (s)."""
    y = np.asarray(y, dtype=float)
    vlo, vhi = levels(y)
    a = vlo + lo * (vhi - vlo)
    b = vlo + hi * (vhi - vlo)
    ta = edges_at(y, dt, a)
    tb = edges_at(y, dt, b)
    j = np.searchsorted(tb, ta)
    ok = j < tb.size
    d = tb[j[ok]] - ta[ok]
    d = d[d > 0]
    if d.size == 0:
        raise AnalysisError("no rising edges")
    return float(np.median(d))


def edges_at(y, dt, level):
    """Rising crossings of an arbitrary level (linear interpolation), without hysteresis."""
    y = np.asarray(y, dtype=float)
    c = np.where((y[:-1] < level) & (y[1:] >= level))[0]
    return (c + (level - y[c]) / (y[c + 1] - y[c])) * dt


def dominant_frequency(y, dt, f_min=0.0):
    """Frequency of the strongest spectral line of y above f_min (FFT with Hann window)."""
    y = np.asarray(y, dtype=float) - np.mean(y)
    w = np.hanning(y.size)
    sp = np.abs(np.fft.rfft(y * w))
    fr = np.fft.rfftfreq(y.size, dt)
    m = fr > f_min
    return float(fr[m][np.argmax(sp[m])])


def vpp(y):
    lo, hi = levels(y)
    return hi - lo


# --------------------------------------------------------------------------- frequency helpers


def bracket_delta(f_meas, f_set, f10a, f10b):
    """Bracket measurement (MESSPLAN.md 3.3): deviation in Hz relative to the 10 MHz readings."""
    return f_meas - f_set * (f10a + f10b) / (2.0 * 10e6)


def ppm(f, f_nom):
    return (f - f_nom) / f_nom * 1e6


def allan_deviation(y, tau0, taus=None):
    """Overlapping Allan deviation of fractional frequency values y sampled every tau0 seconds.

    Returns a list of (tau, adev, n_terms) for the requested averaging factors (default
    1, 2, 5, 10, 20, 50, 100 ... while enough data).
    """
    y = np.asarray(y, dtype=float)
    n = y.size
    if taus is None:
        ms = []
        m = 1
        while 2 * m < n:
            for k in (1, 2, 5):
                if 2 * m * k < n:
                    ms.append(m * k)
            m *= 10
        ms = sorted(set(ms))
    else:
        ms = sorted(set(max(1, int(round(t / tau0))) for t in taus))
    out = []
    x = np.concatenate(([0.0], np.cumsum(y))) * tau0  # phase
    for m in ms:
        if 2 * m >= n + 1:
            continue
        d = x[2 * m:] - 2 * x[m:-m] + x[:-2 * m]
        if d.size < 1:
            continue
        avar = float(np.sum(d ** 2)) / (2.0 * (m * tau0) ** 2 * d.size)
        out.append((m * tau0, math.sqrt(avar), int(d.size)))
    return out


def drift_interpolate(t_ref, f_ref, t):
    """Linear interpolation of the 10 MHz reference readings (bracketing within a sweep)."""
    return float(np.interp(t, t_ref, f_ref))


# --------------------------------------------------------------------------- spectrum


def peak(freqs, levels_dbm, f_center=None, window=None):
    """Highest point (f, dBm), optionally only within f_center +- window."""
    f = np.asarray(freqs, dtype=float)
    p = np.asarray(levels_dbm, dtype=float)
    if f_center is not None and window is not None:
        m = np.abs(f - f_center) <= window
        if not m.any():
            raise AnalysisError("window outside the trace")
        f, p = f[m], p[m]
    i = int(np.argmax(p))
    return float(f[i]), float(p[i])


def spurs(freqs, levels_dbm, f_carrier, exclude, n=3):
    """Largest n lines outside f_carrier +- exclude, relative to the carrier.

    Returns (carrier_dbm, [(offset_hz, dbc), ...]) sorted by level. Local maxima only, so the
    flanks of one line count once.
    """
    f = np.asarray(freqs, dtype=float)
    p = np.asarray(levels_dbm, dtype=float)
    _, carrier = peak(f, p, f_carrier, exclude)
    loc = np.where((p[1:-1] > p[:-2]) & (p[1:-1] >= p[2:]))[0] + 1
    loc = loc[np.abs(f[loc] - f_carrier) > exclude]
    loc = loc[np.argsort(p[loc])[::-1]][:n]
    return carrier, [(float(f[i] - f_carrier), float(p[i] - carrier)) for i in loc]


def harmonics(freqs, levels_dbm, f0, n_max, window):
    """Level of the fundamental and harmonics 2 ... n_max (max within +- window).

    Returns list of (n, f, dBm, dBc); entries outside the trace are left out.
    """
    out = []
    f = np.asarray(freqs, dtype=float)
    fund = None
    for n in range(1, n_max + 1):
        fn = n * f0
        if fn - window < f[0] or fn + window > f[-1]:
            continue
        _, lv = peak(freqs, levels_dbm, fn, window)
        if n == 1:
            fund = lv
        out.append((n, fn, lv, None if fund is None else lv - fund))
    return out


def ideal_square_harmonic_dbc(n, duty=0.5):
    """Level of harmonic n of an ideal square wave with the given duty cycle, relative to n = 1."""
    a1 = abs(math.sin(math.pi * duty))
    an = abs(math.sin(math.pi * n * duty)) / n
    if an < 1e-12:  # even harmonics of an exact 50 % square wave
        return float("-inf")
    return 20.0 * math.log10(an / a1)


def band_edges(freqs, levels_dbm, drop_db=10.0):
    """Lower and upper frequency where a flat band (spread spectrum) falls drop_db below its mean."""
    f = np.asarray(freqs, dtype=float)
    p = np.asarray(levels_dbm, dtype=float)
    top = np.percentile(p, 95)
    inside = np.where(p >= top - drop_db)[0]
    if inside.size == 0:
        raise AnalysisError("no band")
    return float(f[inside[0]]), float(f[inside[-1]])
