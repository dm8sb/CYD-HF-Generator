# SPDX-FileCopyrightText: 2026 DM8SB
# SPDX-License-Identifier: GPL-3.0-or-later

"""The generator under test, driven only by the documented SCPI commands (docs/REMOTE_API.md 3.3).

Every write is followed by SYST:ERR? until the queue is empty; an entry raises DutError. Writes
are spaced so the firmware limit of 20 writes per second and source is never reached.
"""
import time

WRITE_GAP_S = 0.06  # <= 16.7 writes/s, below the firmware limit of 20/s per source


class DutError(RuntimeError):
    pass


def _num(text):
    return float(text.strip().split(",")[0])


class Dut:
    def __init__(self, link):
        self.link = link
        self._last_write = 0.0

    # ------------------------------------------------------------------ plumbing

    def write(self, cmd, check=True):
        wait = self._last_write + WRITE_GAP_S - time.monotonic()
        if wait > 0:
            time.sleep(wait)
        self.link.write(cmd)
        self._last_write = time.monotonic()
        if check:
            errs = self.errors()
            if errs:
                raise DutError("{} -> {}".format(cmd, "; ".join(errs)))

    def query(self, cmd):
        return self.link.query(cmd)

    def errors(self):
        out = []
        for _ in range(17):
            ans = self.link.query("SYST:ERR?")
            if ans.startswith("0,") or ans.startswith("+0,"):
                break
            out.append(ans)
        return out

    # ------------------------------------------------------------------ identity and state

    def idn(self):
        return self.query("*IDN?")

    def info(self):
        idn = self.idn().split(",")
        return {
            "idn": ",".join(idn),
            "mac": idn[2] if len(idn) > 2 else "",
            "version": idn[3] if len(idn) > 3 else "",
            "si5351": self.query("SYST:SI5351?"),
            "correction_ppm": _num(self.query("SYST:CORR?")),
            "remote_outputs": self.query("OUTP:REM?"),
            "heap": self.query("SYST:HEAP?"),
        }

    def reset(self):
        self.write("*RST")

    def standard(self, correction_ppm=None):
        """Standard conditions of MESSPLAN.md 3.2: 8 mA, Fractional, no spread, off state Low."""
        self.reset()
        self.drive(8)
        self.tuning("FRAC")
        self.ssc("OFF")
        self.offstate("LOW")
        if correction_ppm is not None:
            self.correction(correction_ppm)

    # ------------------------------------------------------------------ channels

    def freq(self, ch, hz):
        self.write("SOUR{}:FREQ {:.0f}".format(ch, round(hz)))

    def freq_get(self, ch):
        return _num(self.query("SOUR{}:FREQ?".format(ch)))

    def freq_actual(self, ch):
        return _num(self.query("SOUR{}:FREQ:ACT?".format(ch)))

    def output(self, ch, on):
        self.write("OUTP{} {}".format(ch, "ON" if on else "OFF"))

    def all_off(self):
        for ch in (1, 2, 3):
            self.output(ch, False)

    def condition(self, ch):
        return self.query("OUTP{}:COND?".format(ch)).strip()

    def wait_on(self, ch, timeout_s=5.0):
        end = time.monotonic() + timeout_s
        while True:
            c = self.condition(ch)
            if c == "ON":
                return
            if time.monotonic() > end:
                raise DutError("CH{} is {} instead of ON".format(ch, c))
            time.sleep(0.2)

    def iq(self, master, phase):
        self.write("SOUR3:IQ:MAST {}".format(master))
        self.write("SOUR3:PHAS {}".format(int(phase)))

    def couple_ch2(self, on, offset_hz=0):
        self.write("SOUR2:COUP {}".format("CH1" if on else "OFF"))
        if on:
            self.write("SOUR2:FREQ:OFFS {:.0f}".format(offset_hz))

    # ------------------------------------------------------------------ configuration

    def drive(self, ma):
        self.write("OUTP:DRIV {}".format(int(ma)))

    def tuning(self, mode):
        self.write("SYST:TUN {}".format(mode))

    def ssc(self, mode):
        self.write("SSC {}".format(mode))

    def offstate(self, mode):
        self.write("OUTP:OFFS {}".format(mode))

    def correction(self, ppm_value):
        self.write("SYST:CORR {:.2f}".format(ppm_value))

    def correction_get(self):
        return _num(self.query("SYST:CORR?"))

    def step(self, hz):
        self.write("SYST:STEP {:.0f}".format(hz))

    def backlight(self, pct):
        self.write("SYST:BACK {}".format(int(pct)))

    def save(self):
        self.write("SYST:SAVE")

    def remote_allowed(self):
        return self.query("OUTP:REM?").strip() == "ALLOWED"

    def pulse_reset(self):
        self.link.pulse_reset()
