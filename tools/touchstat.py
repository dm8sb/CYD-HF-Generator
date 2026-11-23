#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Evaluates the touch test logs of MESSPLAN B8, B9, B12, B13 (AF39). Standard library only.

Reads a serial log (one or more files, or stdin) of the test builds and prints:
- [HITMAP] lines of env esp32-2432S028R-hitmap ("target n (x,y) hit (x,y) dx +a dy +b"): per
  target the number of taps, mean and largest deviation, and the share of taps farther than
  --tolerance pixels (default 8, the check cross tolerance of the calibration);
- [TOUCH] lines of env esp32-2432S028R-touchlog ("x=.. y=.. to ..,.. z=.. t=..ms gap=..ms
  drop=n/..ms target=..."): number of touches, press duration, pressure, short gaps between two
  touches (possible double taps, < --double-ms), dropouts within a touch, and the touches per
  target widget.

    python tools/touchstat.py hitmap.log
    python tools/touchstat.py --csv targets.csv session1.log session2.log
"""
import argparse
import csv
import math
import re
import statistics
import sys

HIT_RE = re.compile(r"\[HITMAP\] target (\d+) \((-?\d+),(-?\d+)\) hit \((-?\d+),(-?\d+)\) dx ([+-]?\d+) dy ([+-]?\d+)")
TOUCH_RE = re.compile(r"\[TOUCH\] x=(-?\d+) y=(-?\d+) to (-?\d+),(-?\d+) z=(\d+) t=(\d+)ms gap=(\d+)ms "
                      r"drop=(\d+)/(\d+)ms(?: target=(.*))?")


def parse(lines):
    hits, touches = [], []
    for line in lines:
        m = HIT_RE.search(line)
        if m:
            n, tx, ty, hx, hy, dx, dy = (int(v) for v in m.groups())
            hits.append({"target": n, "tx": tx, "ty": ty, "hx": hx, "hy": hy, "dx": dx, "dy": dy,
                         "dist": math.hypot(dx, dy)})
            continue
        m = TOUCH_RE.search(line)
        if m:
            g = m.groups()
            touches.append({"x": int(g[0]), "y": int(g[1]), "x1": int(g[2]), "y1": int(g[3]), "z": int(g[4]),
                            "t": int(g[5]), "gap": int(g[6]), "drops": int(g[7]), "drop_ms": int(g[8]),
                            "target": (g[9] or "").strip()})
    return hits, touches


def hit_report(hits, tolerance):
    rows = []
    for n in sorted({h["target"] for h in hits}):
        hs = [h for h in hits if h["target"] == n]
        d = [h["dist"] for h in hs]
        rows.append({"target": n, "x": hs[0]["tx"], "y": hs[0]["ty"], "taps": len(hs),
                     "mean_dx": statistics.mean(h["dx"] for h in hs), "mean_dy": statistics.mean(h["dy"] for h in hs),
                     "mean_dist": statistics.mean(d), "max_dist": max(d),
                     "misses": sum(1 for v in d if v > tolerance)})
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("logs", nargs="*", help="log files (default: stdin)")
    ap.add_argument("--tolerance", type=float, default=8.0, help="pixels for a miss in the hit map (default 8)")
    ap.add_argument("--double-ms", type=int, default=150, help="a gap below this counts as a possible double tap")
    ap.add_argument("--csv", metavar="FILE", help="also write the per-target hit map table as CSV")
    args = ap.parse_args()
    lines = []
    if args.logs:
        for name in args.logs:
            with open(name, encoding="utf-8", errors="replace") as f:
                lines.extend(f)
    else:
        lines = sys.stdin.readlines()
    hits, touches = parse(lines)
    if not hits and not touches:
        print("no [HITMAP] or [TOUCH] lines found", file=sys.stderr)
        return 1

    if hits:
        rows = hit_report(hits, args.tolerance)
        print("Hit map: %d taps on %d targets, tolerance %.0f px" % (len(hits), len(rows), args.tolerance))
        print("  target   x,y      taps  mean dx  mean dy  mean dist  max dist  misses")
        for r in rows:
            print("  %6d  %3d,%-3d  %5d  %+7.1f  %+7.1f  %9.1f  %8.1f  %6d" % (
                r["target"], r["x"], r["y"], r["taps"], r["mean_dx"], r["mean_dy"], r["mean_dist"], r["max_dist"], r["misses"]))
        alld = [h["dist"] for h in hits]
        miss = sum(r["misses"] for r in rows)
        print("  all: mean %.1f px, median %.1f px, max %.1f px, misses %d of %d (%.0f %%)" % (
            statistics.mean(alld), statistics.median(alld), max(alld), miss, len(hits), 100.0 * miss / len(hits)))
        if args.csv:
            with open(args.csv, "w", newline="", encoding="utf-8") as f:
                w = csv.DictWriter(f, fieldnames=list(rows[0].keys()), delimiter=";")
                w.writeheader()
                w.writerows(rows)

    if touches:
        t = [x["t"] for x in touches]
        z = [x["z"] for x in touches]
        doubles = sum(1 for x in touches if x["gap"] < args.double_ms)
        dropped = sum(1 for x in touches if x["drops"])
        print("Touches: %d, press %d..%d ms (median %d), pressure median %d, gaps < %d ms: %d, with dropouts: %d (%d in all, longest %d ms)" % (
            len(touches), min(t), max(t), statistics.median(t), statistics.median(z), args.double_ms, doubles, dropped,
            sum(x["drops"] for x in touches), max(x["drop_ms"] for x in touches)))
        per = {}
        for x in touches:
            per[x["target"] or "-"] = per.get(x["target"] or "-", 0) + 1
        for name, n in sorted(per.items(), key=lambda kv: -kv[1]):
            print("  %5d  %s" % (n, name))
    return 0


if __name__ == "__main__":
    sys.exit(main())
