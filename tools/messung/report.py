# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Protocol with figures (MESSPLAN_SIGLENT.md 10.2, 10.3) from one campaign directory.

- every data/<name>.csv with a '# plot:' line becomes img/<name>.png
- messungen/<campaign>.md: header, instruments, setup photos, uncertainties, summary per M-ID,
  one section per step with its values, figures and screenshots, hand measurements, findings
- blocks between <!-- hand:<id> --> and <!-- /hand:<id> --> are kept when the protocol is
  generated again, so hand entries survive
"""
import collections
import csv
import json
import os
import re
import subprocess

import steps as steps_mod
from store import FAIL, INFO, NA, PASS, UNDECIDED

RANK = {FAIL: 5, UNDECIDED: 4, PASS: 3, INFO: 2, NA: 1, "": 0}

STYLE = {
    "figure.figsize": (6.4, 3.6),
    "figure.dpi": 100,
    "font.size": 9,
    "axes.grid": True,
    "grid.alpha": 0.3,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "savefig.bbox": "tight",
}


# =========================================================================== figures


def read_data(path):
    header = {}
    rows = []
    with open(path, encoding="utf-8") as fh:
        lines = fh.read().splitlines()
    body = []
    for line in lines:
        if line.startswith("# "):
            key, _, val = line[2:].partition(": ")
            header[key] = val
        else:
            body.append(line)
    reader = csv.reader(body, delimiter=";")
    names = next(reader)
    cols = collections.OrderedDict((n, []) for n in names)
    for r in reader:
        for n, v in zip(names, r):
            cols[n].append(v)
        rows.append(r)
    plot = json.loads(header["plot"]) if "plot" in header else None
    return header, cols, plot


def _nums(values):
    out = []
    for v in values:
        try:
            out.append(float(v))
        except ValueError:
            out.append(float("nan"))
    return out


def render(csv_path, png_path):
    """Draw one data file; returns False when it has no plot spec."""
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    header, cols, spec = read_data(csv_path)
    if not spec:
        return False
    with plt.rc_context(STYLE):
        kind = spec.get("kind", "line")
        x_raw = cols[spec["x"]]
        if kind == "bar":
            fig, ax = plt.subplots(layout="constrained")
            series = spec["series"]
            n = len(series)
            width = 0.8 / n
            for i, s in enumerate(series):
                xs = [k + (i - (n - 1) / 2) * width for k in range(len(x_raw))]
                ax.bar(xs, _nums(cols[s]), width, label=s)
            ax.set_xticks(range(len(x_raw)))
            ax.set_xticklabels(x_raw, rotation=0)
            ax.legend(fontsize=7)
            axes = [ax]
        elif "subplots" in spec:
            groups = spec["subplots"]
            fig, axes = plt.subplots(len(groups), 1, sharex=True, layout="constrained",
                                     figsize=(6.4, 1.3 + 1.1 * len(groups)))
            if len(groups) == 1:
                axes = [axes]
            x = _nums(x_raw)
            for ax, group, title in zip(axes, groups, spec.get("subtitles", [""] * len(groups))):
                for s in group:
                    ax.plot(x, _nums(cols[s]), lw=1.0, label=s)
                ax.set_title(title, fontsize=9, loc="left")
                ax.set_ylabel(spec.get("ylabel", ""))
                if len(group) > 1:
                    ax.legend(fontsize=7, loc="upper right")
        else:
            fig, ax = plt.subplots(layout="constrained")
            x = _nums(x_raw)
            for s in spec["series"]:
                y = _nums(cols[s])
                if kind == "scatter":
                    err = _nums(cols[spec["err"]]) if spec.get("err") in cols else None
                    ax.errorbar(x, y, yerr=err, fmt="o", ms=3, lw=0.8, capsize=2, label=s)
                else:
                    pts = [(a, b) for a, b in zip(x, y) if b == b]
                    ax.plot([a for a, _ in pts], [b for _, b in pts], lw=1.0 if kind != "line" else 1.4,
                            marker="." if kind == "line" and len(pts) < 60 else None, label=s)
            if spec.get("limit_series") in cols:
                lim = _nums(cols[spec["limit_series"]])
                ax.step(x, lim, where="mid", color="0.4", lw=0.8, ls="--", label="limit")
                ax.step(x, [-v for v in lim], where="mid", color="0.4", lw=0.8, ls="--")
            for v in spec.get("limits", []):
                ax.axhline(v, color="0.4", lw=0.8, ls="--")
            if spec.get("xscale"):
                ax.set_xscale(spec["xscale"])
            if spec.get("yscale"):
                ax.set_yscale(spec["yscale"])
            if len(spec["series"]) > 1 or spec.get("limit_series"):
                ax.legend(fontsize=7)
            axes = [ax]
        axes[-1].set_xlabel(spec.get("xlabel", ""))
        if kind != "wave" or "subplots" not in spec:
            axes[0].set_ylabel(spec.get("ylabel", ""))
        fig.suptitle(spec.get("title", ""), fontsize=10, x=0.02, ha="left")
        fig.savefig(png_path)
        plt.close(fig)
    return True


# =========================================================================== protocol


def _hand_blocks(text):
    return {m.group(1): m.group(2) for m in
            re.finditer(r"<!-- hand:([\w.-]+) -->\n(.*?)<!-- /hand:\1 -->", text, re.S)}


def _hand(blocks, key, default):
    body = blocks.get(key, default.rstrip("\n") + "\n")
    return "<!-- hand:{} -->\n{}<!-- /hand:{} -->".format(key, body, key)


def _esc(v):
    return str(v).replace("|", "\\|").replace("\n", " ")


def _git_commit():
    try:
        return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], stderr=subprocess.DEVNULL,
                                       cwd=os.path.dirname(os.path.abspath(__file__))).decode().strip()
    except Exception:
        return "unknown"


def generate(session_dir):
    """Figures and protocol for a campaign directory; returns the protocol path."""
    base = os.path.dirname(os.path.abspath(session_dir))
    name = os.path.basename(os.path.abspath(session_dir))
    proto = os.path.join(base, name + ".md")
    meta = {}
    mp = os.path.join(session_dir, "meta.json")
    if os.path.exists(mp):
        with open(mp, encoding="utf-8") as fh:
            meta = json.load(fh)
    results = []
    rp = os.path.join(session_dir, "results.csv")
    if os.path.exists(rp):
        with open(rp, encoding="utf-8") as fh:
            results = list(csv.DictReader(fh, delimiter=";"))
    old = ""
    if os.path.exists(proto):
        with open(proto, encoding="utf-8") as fh:
            old = fh.read()
    hand = _hand_blocks(old)

    # figures
    figs = collections.defaultdict(list)
    data_dir = os.path.join(session_dir, "data")
    for fn in sorted(os.listdir(data_dir)) if os.path.isdir(data_dir) else []:
        if not fn.endswith(".csv"):
            continue
        png = os.path.join(session_dir, "img", fn[:-4] + ".png")
        header, _, spec = read_data(os.path.join(data_dir, fn))
        if spec and render(os.path.join(data_dir, fn), png):
            figs[header.get("step", "")].append((fn[:-4], spec.get("title", fn[:-4])))
    img_rel = name + "/img/"
    screens = collections.defaultdict(list)
    img_dir = os.path.join(session_dir, "img")
    for fn in sorted(os.listdir(img_dir)) if os.path.isdir(img_dir) else []:
        if fn.endswith("_screen.png"):
            screens[fn.split("_", 1)[0]].append(fn)

    by_step = collections.OrderedDict()
    for r in results:
        by_step.setdefault(r["step"], []).append(r)

    dut = meta.get("dut", {})
    cal = meta.get("cal", {})
    out = []
    w = out.append
    w("# Messprotokoll {}".format(name))
    w("")
    w("Erzeugt von `tools/messung/report.py` (Skript-Commit {}) nach [MESSPLAN_SIGLENT.md](../MESSPLAN_SIGLENT.md); "
      "Sollwerte und Bewertung nach [MESSPLAN.md](../MESSPLAN.md) 3.6. Abschnitte zwischen `hand:`-Markierungen "
      "werden beim erneuten Erzeugen nicht überschrieben.".format(_git_commit()))
    w("")
    w("## 1. Kopf")
    w("")
    w("| Angabe | Wert |")
    w("|---|---|")
    w("| Kampagne | {} |".format(name))
    w("| Prüfling | {} |".format(_esc(dut.get("idn", ""))))
    w("| SI5351 | {} |".format(dut.get("si5351", "")))
    w("| Korrektur nach L1.2 | {} ppm |".format(meta.get("correction_ppm", cal.get("corr", "–"))))
    w("| Raumtemperatur Anfang | {} °C |".format(meta.get("temperature_start", "–")))
    w("| Simulation | {} |".format("ja (Trockenlauf, keine Messwerte des Geräts)" if meta.get("simulated") else "nein"))
    w("")
    w(_hand(hand, "head", "| Angabe | Wert |\n|---|---|\n| Bediener | |\n| Versorgung (Isolator / Powerbank) | |\n"
          "| Variante der Referenzkette (3.2) | |\n| Firmware SHA-256 | |\n| Raumtemperatur Ende | |\n"))
    w("")
    w("## 2. Messgeräte")
    w("")
    w("| Gerät | *IDN? |")
    w("|---|---|")
    for k, v in meta.get("instruments", {}).items():
        w("| {} | {} |".format(k, _esc(v)))
    w("")
    w(_hand(hand, "instruments", "Letzte Kalibrierung der Messgeräte, GPS-Lock:\n"))
    w("")
    w("## 3. Aufbau")
    w("")
    w("Gesamtaufbau G nach MESSPLAN_SIGLENT.md 3.1; Steckvarianten in der Reihenfolge des Laufs:")
    w("")
    plugs = [p for p in meta.get("plugs", [])]
    if plugs:
        w(", ".join("+".join(p["plug"]) for p in plugs))
        w("")
    for ph in meta.get("photos", []):
        path = os.path.join(session_dir, "img", ph["file"])
        if os.path.exists(path):
            w("![{}]({}{})".format(ph["text"], img_rel, ph["file"]))
        else:
            w("- Foto fehlt: {} (`{}`)".format(ph["text"], img_rel + ph["file"]))
    w("")
    w("## 4. Unsicherheiten und Eigenwerte")
    w("")
    w("| Größe | Wert |")
    w("|---|---|")
    labels = [("u_f_rel", "u_f (relativ)"), ("u_t", "u_t (s)"), ("gps_vpp", "GPS an der Kette (Vpp)"),
              ("harm_atten", "Eingangsdämpfung für Oberwellen (dB)"), ("ssa_danl", "Rauschboden SSA (dBm/Hz)"),
              ("eps0_ppm", "Quarzfehler (ppm)"), ("corr", "Korrektur (ppm)"), ("adc_pairs_ok", "ADC-Paare K1/K2, K3/K4")]
    for key, label in labels:
        if key in cal:
            v = cal[key]
            w("| {} | {} |".format(label, "{:.4g}".format(v) if isinstance(v, float) else v))
    if cal.get("dt0"):
        w("| Δt₀ (ps) je Frequenz | {} |".format(", ".join("{:.0f} MHz: {:.1f}".format(float(k) / 1e6, v * 1e12)
                                                           for k, v in sorted(cal["dt0"].items(), key=lambda i: float(i[0])))))
    w("")
    w("## 5. Zusammenfassung")
    w("")
    per_mid = collections.OrderedDict()
    for r in results:
        if not r["mid"]:
            continue
        for mid in r["mid"].split(","):
            cur = per_mid.get(mid.strip())
            if cur is None or RANK.get(r["verdict"], 0) > RANK.get(cur["verdict"], 0):
                per_mid[mid.strip()] = r
    counts = collections.Counter(r["verdict"] for r in per_mid.values())
    w("Je Mess-ID die schlechteste Bewertung aller Werte: ✔ {} · ✘ {} · ○ {} · i {} · offen {}".format(
        counts.get(PASS, 0), counts.get(FAIL, 0), counts.get(UNDECIDED, 0), counts.get(INFO, 0), counts.get("", 0)))
    w("")
    w("| Mess-ID | Bewertung | maßgeblicher Wert | Schritt |")
    w("|---|---|---|---|")
    for mid in sorted(per_mid):
        r = per_mid[mid]
        val = "{} {} {}".format(r["quantity"], r["value"], r["unit"]).strip()
        w("| {} | {} | {} | [{}](#{}) |".format(mid, r["verdict"] or "offen", _esc(val), r["step"],
                                                _anchor(r["step"])))
    w("")
    w("## 6. Messungen")
    w("")
    manual_rows = []
    for sid, st in steps_mod.STEPS.items():
        rows = by_step.get(sid)
        if not rows and not figs.get(sid):
            continue
        w("### {} – {}".format(sid, st.title))
        w("")
        if st.mids:
            w("Liefert: {}".format(", ".join(st.mids)))
            w("")
        if rows:
            w("| Größe | Soll | Messwert | u | Einheit | Grenze | Bewertung | Bemerkung |")
            w("|---|---|---|---|---|---|---|---|")
            for r in rows:
                if r["note"].startswith("by hand"):
                    manual_rows.append(r)
                w("| {} | {} | {} | {} | {} | {} | {} | {} |".format(
                    _esc(r["quantity"]), _short(r["setpoint"]), _short(r["value"]), _short(r["u"]), r["unit"],
                    _short(r["limit"]), r["verdict"] or "offen", _esc(r["note"])))
            w("")
        for fig, title in figs.get(sid, []):
            w("![{}]({}{}.png)".format(_esc(title), img_rel, fig))
            w("")
        shots = [s for s in screens.get(sid, [])] + [s for k, v in screens.items() if k != sid
                                                    and k.startswith(sid + "_") for s in v]
        if shots:
            w("Bildschirmfotos: " + ", ".join("[{}]({}{})".format(s[:-4], img_rel, s) for s in shots))
            w("")
        if sid == "L6.5" or any(r["note"].startswith("by hand") for r in rows or []):
            w(_hand(hand, "step-" + sid, "Handeinträge {}:\n".format(sid)))
            w("")
    w("## 7. Handmessungen")
    w("")
    if manual_rows:
        w("| Schritt | Mess-ID | Was | Ergebnis |")
        w("|---|---|---|---|")
        for r in manual_rows:
            w("| {} | {} | {} | siehe Handeintrag {} |".format(r["step"], r["mid"], _esc(r["quantity"]), r["step"]))
        w("")
    w(_hand(hand, "manual", "Weitere Handmessungen (Touch, Speichern, Display; MESSPLAN.md Stufe 1):\n"))
    w("")
    w("## 8. Befunde")
    w("")
    fails = [r for r in results if r["verdict"] == FAIL]
    if fails:
        w("Automatisch als ✘ bewertet (je Eintrag ein Befund MB-n nach MESSPLAN.md Anhang E):")
        w("")
        for r in fails:
            w("- {} {}: {} = {} {} (Grenze {})".format(r["step"], r["mid"], r["quantity"], r["value"], r["unit"], r["limit"]))
        w("")
    w(_hand(hand, "findings", "Befunde MB-n (Mess-ID, Messwert, Bild, Vermutung, Aufgabe in ARBEITSPLAN.md):\n"))
    w("")
    w("## Anhang: Dateien")
    w("")
    for sub in ("data", "img"):
        d = os.path.join(session_dir, sub)
        files = sorted(os.listdir(d)) if os.path.isdir(d) else []
        w("- `{}/`: {} Dateien".format(sub, len(files)))
    w("")
    with open(proto, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(out))
    return proto


def _short(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return _esc(v)
    if f != f:
        return "nan"
    if f == 0:
        return "0"
    a = abs(f)
    if a >= 1e5 or a < 1e-3:
        return "{:.6g}".format(f)
    return "{:.4g}".format(f) if a < 1 else "{:.6g}".format(f)


def _anchor(sid):
    st = steps_mod.STEPS.get(sid)
    text = "{} – {}".format(sid, st.title) if st else sid
    text = text.lower()
    text = re.sub(r"[^\w\- ]", "", text)
    return text.replace(" ", "-")


def render_docs(data_dir, img_dir):
    """Figures for the manuals: docs/data/meas-*.csv -> docs/img/meas-*.png (section 11)."""
    done = []
    for fn in sorted(os.listdir(data_dir)) if os.path.isdir(data_dir) else []:
        if fn.startswith("meas-") and fn.endswith(".csv"):
            png = os.path.join(img_dir, fn[:-4] + ".png")
            if render(os.path.join(data_dir, fn), png):
                done.append(png)
    return done
