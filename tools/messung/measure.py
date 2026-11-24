#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Automated measurements of MESSPLAN_SIGLENT.md: run steps, check instruments, write the protocol.

    # dry run on the simulated bench (no hardware), protocol in messungen/<name>.md
    python tools/messung/measure.py run all --sim --quick

    # real bench (setup G); the campaign directory is messungen/<date>_<board>_<firmware>/
    python tools/messung/measure.py check --dut usb:COM13 --sds visa:TCPIP::192.168.1.20::INSTR \\
        --ssa visa:TCPIP::192.168.1.21::INSTR
    python tools/messung/measure.py run V L1 L2 --dut usb:COM13 --sds ... --ssa ... --board B
    python tools/messung/measure.py run L5 ...            # next morning, after >= 2 h without power
    python tools/messung/measure.py report messungen/2026-12-01_B_2.1.0
    python tools/messung/measure.py docplots             # docs/data/meas-*.csv -> docs/img/meas-*.png
    python tools/messung/measure.py list

Steps that need plugging or a hand measurement stop and ask; the generator outputs are switched
off before every plug prompt. A failing step is recorded and the run continues with the next one.
"""
import argparse
import datetime
import os
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.dirname(os.path.dirname(HERE))

import links  # noqa: E402
import report  # noqa: E402
import steps  # noqa: E402
from bench import Bench  # noqa: E402
from dut import Dut  # noqa: E402
from siglent import Sdg, Sds, Ssa  # noqa: E402
from store import UNDECIDED, Session  # noqa: E402


def build_bench(args, session_name=None):
    sim = None
    if args.sim:
        import sim as sim_mod

        sim = sim_mod.SimBench(seed=args.seed)
        args.dut = args.dut or "sim:dut"
        args.sds = args.sds or "sim:sds"
        args.ssa = args.ssa or "sim:ssa"
        if args.with_sdg:
            args.sdg = args.sdg or "sim:sdg"
    for k in ("dut", "sds", "ssa"):
        if not getattr(args, k):
            raise SystemExit("--{} is missing (or use --sim)".format(k))
    dut = Dut(links.open_link(args.dut, sim))
    if sim:
        import dut as dut_mod

        dut_mod.WRITE_GAP_S = 0.0
    sds = Sds(links.open_link(args.sds, sim))
    ssa = Ssa(links.open_link(args.ssa, sim))
    sdg = Sdg(links.open_link(args.sdg, sim)) if args.sdg else None
    name = session_name or args.name
    if not name:
        version = dut.idn().split(",")[-1].strip() or "unknown"
        name = "{}_{}_{}".format(datetime.date.today().isoformat(), args.board, version)
    session = Session(args.out, name)
    instruments = {"Prüfling": dut.idn(), "SDS": sds.idn(), "SSA": ssa.idn()}
    if sdg:
        instruments["SDG"] = sdg.idn()
    session.set_meta(instruments=instruments, simulated=bool(sim), board=args.board, record=args.record)
    return Bench(dut, sds, ssa, session, sdg=sdg, sim=sim, record=args.record), session


def cmd_run(args):
    ids = steps.resolve(args.steps)
    if not args.with_sdg and not args.sdg:
        ids = [i for i in ids if i != "V-04b"]
    opts = steps.Options(sweep_points=args.sweep_points, drift_minutes=args.drift_minutes,
                         warmup_minutes=args.warmup_minutes, settle_minutes=args.settle_minutes,
                         resets=args.resets, quick=args.quick)
    if args.quick:
        opts.sweep_points = min(opts.sweep_points, 12)
        opts.drift_minutes = min(opts.drift_minutes, 3)
        opts.warmup_minutes = min(opts.warmup_minutes, 20)
        opts.settle_minutes = 0
    bench, session = build_bench(args)
    print("Campaign {} -> {}".format(session.name, session.dir))
    failed = []
    for sid in ids:
        st = steps.STEPS[sid]
        print("\n== {} {} ({})".format(sid, st.title, ", ".join(st.mids) or "-"))
        t0 = time.monotonic()
        try:
            st.func(bench, opts)
        except KeyboardInterrupt:
            print("\ninterrupted; outputs off")
            _safe_off(bench)
            break
        except Exception as exc:
            failed.append(sid)
            traceback.print_exc()
            session.record(sid, ",".join(st.mids), "step aborted", "", "", verdict_=UNDECIDED,
                           note="{}: {}".format(type(exc).__name__, exc))
            _safe_off(bench)
        print("   {:.1f} s".format(time.monotonic() - t0))
    _safe_off(bench)
    proto = report.generate(session.dir)
    print("\nProtocol: {}".format(proto))
    if failed:
        print("Aborted steps: {}".format(", ".join(failed)))
    return 1 if failed else 0


def _safe_off(bench):
    try:
        bench.dut.all_off()
    except Exception as exc:
        print("  outputs off failed: {}".format(exc))


def cmd_check(args):
    """Send every query of the instruments once; list what does not answer (before V-01)."""
    sim = None
    if args.sim:
        import sim as sim_mod

        sim = sim_mod.SimBench()
        args.dut, args.sds, args.ssa = "sim:dut", "sim:sds", "sim:ssa"
    bad = 0
    targets = [("DUT", args.dut, ["*IDN?", "SYST:SI5351?", "OUTP:REM?", "SYST:CORR?", "SOUR1:FREQ:ACT?"]),
               ("SDS", args.sds, Sds.QUERIES), ("SSA", args.ssa, Ssa.QUERIES)]
    if args.sdg:
        targets.append(("SDG", args.sdg, Sdg.QUERIES))
    for name, spec, queries in targets:
        if not spec:
            print("{}: not given".format(name))
            continue
        try:
            link = links.open_link(spec, sim)
        except Exception as exc:
            print("{}: cannot open {}: {}".format(name, spec, exc))
            bad += 1
            continue
        for q in queries:
            try:
                ans = link.query(q)
                print("{} {:<20} {}".format(name, q, ans[:70]))
            except Exception as exc:
                bad += 1
                print("{} {:<20} NO ANSWER ({})".format(name, q, exc))
        link.close()
    print("\n{} queries without answer".format(bad))
    return 1 if bad else 0


def cmd_report(args):
    print(report.generate(args.dir))
    return 0


def cmd_docplots(args):
    done = report.render_docs(os.path.join(ROOT, "docs", "data"), os.path.join(ROOT, "docs", "img"))
    for p in done:
        print(p)
    print("{} figures".format(len(done)))
    return 0


def cmd_list(args):
    for sid, st in steps.STEPS.items():
        print("{:<6} {:<4} {:<26} {}".format(sid, st.group, ", ".join(st.mids), st.title))
    print("\ngroups: all (= V L1 L2 L3 L4 L6), " + ", ".join(steps.GROUPS))
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    def bench_args(sp):
        sp.add_argument("--dut", help="generator: usb:COM13, tcp:<ip>[:5025], visa:<resource>")
        sp.add_argument("--sds", help="oscilloscope: visa:TCPIP::<ip>::INSTR")
        sp.add_argument("--ssa", help="spectrum analyzer: visa:TCPIP::<ip>::INSTR")
        sp.add_argument("--sdg", help="optional SDG for V-04 b: visa:TCPIP::<ip>::INSTR")
        sp.add_argument("--sim", action="store_true", help="simulated bench (dry run, tests)")

    r = sub.add_parser("run", help="run steps or groups (V, L1 ... L6, all)")
    r.add_argument("steps", nargs="+")
    bench_args(r)
    r.add_argument("--with-sdg", action="store_true", help="include V-04 b (with --sim: simulated SDG)")
    r.add_argument("--out", default=os.path.join(ROOT, "messungen"))
    r.add_argument("--name", help="campaign name (default <date>_<board>_<firmware>)")
    r.add_argument("--board", default="B", help="board label of MESSPLAN M-001 (default B)")
    r.add_argument("--record", default="14M", choices=["14K", "140K", "1.4M", "14M"],
                   help="SDS record length for 2.1 (default 14M = 14 ms; the sweep L1.4 always uses 1.4M)")
    r.add_argument("--sweep-points", type=int, default=200)
    r.add_argument("--drift-minutes", type=int, default=30)
    r.add_argument("--warmup-minutes", type=int, default=60)
    r.add_argument("--settle-minutes", type=int, default=15)
    r.add_argument("--resets", type=int, default=20)
    r.add_argument("--quick", action="store_true", help="short loops (dry run)")
    r.add_argument("--seed", type=int, default=1)
    r.set_defaults(func=cmd_run)

    c = sub.add_parser("check", help="send every instrument query once")
    bench_args(c)
    c.set_defaults(func=cmd_check)

    rep = sub.add_parser("report", help="generate figures and protocol of a campaign directory")
    rep.add_argument("dir")
    rep.set_defaults(func=cmd_report)

    d = sub.add_parser("docplots", help="docs/data/meas-*.csv -> docs/img/meas-*.png")
    d.set_defaults(func=cmd_docplots)

    ls = sub.add_parser("list", help="list the steps")
    ls.set_defaults(func=cmd_list)

    args = p.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
