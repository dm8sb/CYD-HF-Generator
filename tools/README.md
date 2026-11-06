# Host tools

Python 3.8+ helpers for the CYD HF generator. Comments and help texts are in English.

| File | Purpose |
|---|---|
| `scpi_example.py` | Minimal SCPI example: connect, `*IDN?`, CH1 to 7.1 MHz, on, `FREQ:ACT?`, read the error queue, off. |
| `rest_example.py` | Minimal REST example (urllib only): token from the index page, `GET /api/state`, `POST /api/channel/1`. The REST API is **not in the firmware yet**; the script follows `ARBEITSPLAN.md` section 3.6. |
| `device_check.py` | Automated SCPI check for the smoke test and the measurement plan (`MESSPLAN.md` 4.3, L6, L-S). |
| `fake_scpi_device.py` | Fake device (local TCP server, subset of the manual) to try the scripts without hardware. |
| `build_info.py` | PlatformIO build script (not a host tool). |

## Install

```bash
pip install pyserial                # USB
pip install pyvisa pyvisa-py        # optional: PyVISA (LabVIEW, NI-MAX, MATLAB users know the resource strings)
```

TCP and REST need only the standard library.

## Safety

The generator outputs RF square waves with harmonics. Do not connect an antenna without a suitable filter.
`scpi_example.py` and `rest_example.py` switch CH1 on briefly. `device_check.py` switches **no output on**
unless you pass `--outputs`; it changes only RAM state, restores the settings it touched, switches all
outputs off at the end and never sends `SYST:SAVE` (`*RST` only with `--allow-rst`).
Open the USB port with DTR and RTS off, otherwise the board restarts (the scripts do this).

## Examples

```bash
python tools/scpi_example.py --usb COM5
python tools/scpi_example.py --tcp cyd-hf-1a2b.local            # port 5025
python tools/scpi_example.py --visa "TCPIP0::cyd-hf-1a2b.local::5025::SOCKET"   # needs pyvisa
python tools/rest_example.py cyd-hf-1a2b.local --password secret

python tools/device_check.py --usb COM5                          # settings only, outputs stay off
python tools/device_check.py --tcp 192.168.1.50 --repeat 1000    # L6: 1000 x *IDN? timing
python tools/device_check.py --usb COM5 --outputs --json > report.json
```

`device_check.py` prints one `PASS` / `FAIL` / `SKIP` line per check and a summary. Exit code 0 = no FAIL,
1 = at least one FAIL, 2 = no connection. `--json` writes the report to stdout (text lines go to stderr).
The expected values (IDN pattern, limits, error texts, round-trip table, rejected inputs with their error
codes) are in the tables at the top of the script; adjust them when firmware or manual change.

### Offline try-out

```bash
python tools/fake_scpi_device.py --port 5025        # terminal 1
python tools/device_check.py --tcp 127.0.0.1:5025   # terminal 2
```

The fake does not implement `*RST`, so `--allow-rst` reports one FAIL against it; `OUTP:REM?` always answers
`ALLOWED`, so the `-221` check is skipped. Everything else passes against it.
