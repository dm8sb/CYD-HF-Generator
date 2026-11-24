# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""Storage of one measurement campaign (MESSPLAN_SIGLENT.md 10.1).

    messungen/<date>_<board>_<firmware>/
      meta.json      header data, uncertainties, plug log            (local)
      results.csv    one line per value                              (local)
      data/          condensed data per figure as CSV                (committed)
      img/           figures, screenshots, photos                    (committed)
      raw/           raw oscilloscope records (.npz)                 (local)
    messungen/<date>_<board>_<firmware>.md                            protocol (committed)

Every value is written at once, so an interrupted run loses nothing.
"""
import csv
import datetime
import json
import os

import numpy as np

RESULT_FIELDS = ["time", "step", "mid", "quantity", "channel", "setpoint", "value", "u", "unit",
                 "limit", "verdict", "note"]

# verdict symbols of MESSPLAN.md 3.6
PASS, FAIL, UNDECIDED, INFO, NA = "✔", "✘", "○", "i", "–"


def verdict(deviation, u, limit):
    """✔ if |deviation| + u <= limit, ✘ if |deviation| - u > limit, else ○ (MESSPLAN.md 3.6)."""
    if limit is None:
        return INFO
    if abs(deviation) + u <= limit:
        return PASS
    if abs(deviation) - u > limit:
        return FAIL
    return UNDECIDED


class Session:
    def __init__(self, base_dir, name):
        self.dir = os.path.join(base_dir, name)
        self.name = name
        self.protocol = os.path.join(base_dir, name + ".md")
        for sub in ("data", "img", "raw"):
            os.makedirs(os.path.join(self.dir, sub), exist_ok=True)
        self.meta_path = os.path.join(self.dir, "meta.json")
        self.results_path = os.path.join(self.dir, "results.csv")
        self.meta = {}
        if os.path.exists(self.meta_path):
            with open(self.meta_path, encoding="utf-8") as fh:
                self.meta = json.load(fh)

    # ------------------------------------------------------------------ meta

    def set_meta(self, **kw):
        self.meta.update(kw)
        self._save_meta()

    def meta_section(self, section, **kw):
        self.meta.setdefault(section, {}).update(kw)
        self._save_meta()

    def meta_append(self, key, item):
        self.meta.setdefault(key, []).append(item)
        self._save_meta()

    def _save_meta(self):
        tmp = self.meta_path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as fh:
            json.dump(self.meta, fh, indent=2, ensure_ascii=False, default=_jsonable)
        os.replace(tmp, self.meta_path)

    # ------------------------------------------------------------------ values

    def record(self, step, mid, quantity, value, unit="", setpoint=None, u=None, limit=None,
               verdict_=None, channel="", note=""):
        new = not os.path.exists(self.results_path)
        row = {
            "time": now_iso(), "step": step, "mid": mid, "quantity": quantity, "channel": channel,
            "setpoint": _fmt(setpoint), "value": _fmt(value), "u": _fmt(u), "unit": unit,
            "limit": _fmt(limit), "verdict": verdict_ or "", "note": note,
        }
        with open(self.results_path, "a", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=RESULT_FIELDS, delimiter=";")
            if new:
                w.writeheader()
            w.writerow(row)
        print("  {:<6} {:<6} {:<34} {:>16} {:<6} {}".format(step, mid, quantity[:34], row["value"],
                                                           unit, row["verdict"]))
        return row

    def results(self):
        if not os.path.exists(self.results_path):
            return []
        with open(self.results_path, encoding="utf-8") as fh:
            return list(csv.DictReader(fh, delimiter=";"))

    # ------------------------------------------------------------------ data and images

    def save_data(self, name, columns, plot=None, **header):
        """Condensed data for one figure: data/<name>.csv with '# key: value' header lines.

        columns: dict name -> array (same length, or shorter columns padded with blanks).
        plot: dict for report.py: kind (line, scatter, spectrum, bar, wave), title, x, y, series
        (list of y column names), xscale/yscale, limits (list of y values), xlabel, ylabel.
        """
        path = os.path.join(self.dir, "data", name + ".csv")
        names = list(columns)
        cols = [np.asarray(columns[n]).ravel() for n in names]
        n = max(len(c) for c in cols) if cols else 0
        with open(path, "w", newline="", encoding="utf-8") as fh:
            for k, v in header.items():
                fh.write("# {}: {}\n".format(k, v))
            if plot:
                fh.write("# plot: {}\n".format(json.dumps(plot, ensure_ascii=False)))
            w = csv.writer(fh, delimiter=";")
            w.writerow(names)
            for i in range(n):
                w.writerow([_fmt(c[i]) if i < len(c) else "" for c in cols])
        return path

    def save_raw(self, name, **arrays):
        np.savez_compressed(os.path.join(self.dir, "raw", name + ".npz"), **arrays)

    def img(self, name):
        return os.path.join(self.dir, "img", name)

    def save_image(self, name, png_bytes):
        path = self.img(name)
        with open(path, "wb") as fh:
            fh.write(png_bytes)
        return path


def now_iso():
    return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat()


def _fmt(v):
    if v is None:
        return ""
    if isinstance(v, (float, np.floating)):
        if v != v:
            return "nan"
        return "{:.10g}".format(float(v))
    if isinstance(v, (np.integer,)):
        return str(int(v))
    return str(v)


def _jsonable(o):
    if isinstance(o, (np.floating,)):
        return float(o)
    if isinstance(o, (np.integer,)):
        return int(o)
    if isinstance(o, np.ndarray):
        return o.tolist()
    return str(o)
