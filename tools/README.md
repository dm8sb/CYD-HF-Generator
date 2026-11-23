# Host tools

Python 3.8+ helpers for the CYD HF generator. Comments and help texts are in English.

| File | Purpose |
|---|---|
| `scpi_example.py` | Minimal SCPI example: connect, `*IDN?`, CH1 to 7.1 MHz, on, `FREQ:ACT?`, read the error queue, off. |
| `rest_example.py` | Minimal REST example (urllib only): token from the index page, `GET /api/state`, `POST /api/channel/1`. |
| `web_check.py` | Contract check of the web server (S55, S56) against the device or `web_mock.py`: page and headers, guard answers (token, origin, host, sizes), REST error codes, writes answered with the state, rate limit, WiFi settings without keys, scan semantics, WebSocket push and limits, login and logout. Never switches an output on or changes WiFi. |
| `contract_check.py` | Shared contract vectors (AF34): sends every request of `test/contract/vectors.json` to the web server and compares status and SCPI code; the C++ host tests (`test_http_guard`, `test_rest_json`) read the same file, so a drift of mock, firmware or contract shows on both sides. CI runs it against `web_mock.py`, S61 against the device (no web password). |
| `device_check.py` | Automated SCPI check for the smoke test and the measurement plan (`MESSPLAN.md` 4.3, L6, L-S); `--profile L6` (1000 × `*IDN?`, median < 50 ms) and `--profile L22` (every query of the command tree followed by `SYST:ERR?`, valid and invalid parameters, `*RST`). |
| `slowloris.py` | Slow-request load of MESSPLAN L-S5 (AF39), only against your own device: N connections (default 30) send a header byte every 2 s and complete after 10 s; prints per connection accepted, refused, closed by the server (when) or answered. |
| `touchstat.py` | Evaluation of the touch test logs (MESSPLAN B8, B9, B12, B13, AF39): `[HITMAP]` deviation per target and miss rate, `[TOUCH]` press time, pressure, double-tap gaps, dropouts, touches per widget; `--csv` writes the target table. |
| `counter_log.py` | Frequency counter logger of MESSPLAN C7, C8 (AF39): a SCPI query over TCP or USB (pyserial) in the rhythm of the gate time into `time;frequency;temperature` CSV. |
| `lxi_example.py` | LXI / VXI-11 example (standard library): discovery by portmapper broadcast like `lxi discover`, `*IDN?` and commands over VXI-11, optionally through PyVISA (`TCPIP::<ip>::INSTR`). |
| `fake_scpi_device.py` | Fake device (local TCP server, subset of the manual incl. `*RST`) to try the scripts without hardware; `--vxi11` adds a reduced VXI-11 server (portmapper and core channel 9009). |
| `size_report.py` | Flash/RAM of the firmware env from the ELF as `size.json` and a Markdown table with the difference to a baseline (CI job summary, G7). Local: `python tools/size_report.py measure -e esp32-2432S028R` after `pio run`; `--self-test` runs its tests. |
| `hostport.py` | The one `host[:port]` parser of the tools (`web_check.py`, `device_check.py`, `scpi_example.py`, the Host check of `web_mock.py`): `[v6]:port` with brackets, a bad port is an error instead of a guess; `python tools/hostport.py` runs its self-test. |
| `device_constants.py` | Frequency limits, step list, correction range and defaults read from `include/constants.h`, for `web_mock.py`, `fake_scpi_device.py` and `web_check.py` (no copies of firmware constants in the tools). |
| `web_mock.py` | Mock of the web server (standard library): serves `data/www` with the REST API, WebSocket and request guard on a simulated device, to try the web UI and `web_check.py` without hardware (see below). |
| `flasher_site.py` | Release helper: collects the flash parts and offsets from PlatformIO, the license notices and LGPL sources, downloads ESP Web Tools by hash and assembles and checks the web flasher site (used by `release.yml`; `--help` per subcommand). |
| `release_files.py` | Release helper (AF23): `name` turns the build artifact into the release assets (`cyd-hf-generator-<version>-…`, deterministic licenses archive, LGPL sources, `SHA256SUMS`), `notes` writes the release notes with offsets, esptool command and package list; a missing file or part fails. `release.yml` runs it for a tag and in the dry run of pull requests to `main`. |
| `dep_check.py` | Pinned dependencies (AF22): `packages` compares `pio pkg list` and the build metadata with the exact pins in `platformio.ini` (platform and `platform_packages` by release URL with the SHA-256 digests GitHub publishes, recorded in `ASSET_SHA256`; toolchain and esptool as the platform names them; registry libraries) and with the versions in `THIRD_PARTY_NOTICES.md` (Arduino-ESP32, ESP-IDF tag and commit, GCC runtime, libraries); CI and `release.yml` run it, `--summary` writes the package table. `advisories` (AF26) checks the pinned versions (libraries, Arduino-ESP32 and ESP-IDF from the notices, ESP Web Tools and its npm packages) against the GitHub advisories of the upstream repositories and OSV; a finding fails (`advisories.yml` weekly with an issue, `release.yml` before every release). Advisories that do not apply go with their reason into `advisories_accepted.json`; `--override NAME=VERSION` tests with an old version (`--override ESPAsyncWebServer=3.11.0` finds two advisories); token from `GITHUB_TOKEN`/`GH_TOKEN`. |
| `advisories_accepted.json` | Advisories of pinned dependencies that do not apply to this firmware, each with the reason (read by `dep_check.py advisories`). |
| `requirements-ci.txt` | PlatformIO Core and its Python dependencies with hashes for the workflows (`pip install --require-hashes -r tools/requirements-ci.txt`); Dependabot keeps it current. |
| `prefix_map.py` | PlatformIO build script (not a host tool): maps the build paths in `__FILE__` to neutral prefixes, so no local path ends up in the image. |
| `render_images.py` | Renders the images in `docs/img/`: device screens from the real GUI code (host renderer `screens/`, env `screens-win`/`screens`), the case (OpenSCAD), the web UI on `web_mock.py` and the social preview; sample values only. Paths can be set with `PIO`, `OPENSCAD`, `CHROME`, `NODE`. Screens: `pio run -e screens-win` (`screens` on Linux/macOS) builds a host program from `src/gui` with LVGL 8.4 and the fakes of `test/fakes`, RGB565 frames are scaled 2× into a bezel; case: OpenSCAD 2025 (`--backend=Manifold`), headless Chrome or Edge scales down; web: `web_mock.py` on a free port of 127.0.0.1 set over REST. |
| `cdp_shot.mjs` | Node 22+ helper of `render_images.py`: screenshots over the Chrome DevTools protocol (viewport and phone emulation, clip to an element). |
| `screens/` | Host screen renderer (not a host tool): stand-ins for display, touch, WiFi, LED and build info, the sample state and `lv_conf_host.h`; built by `pio run -e screens-win`. |
| `build_info.py` | PlatformIO build script (not a host tool). |
| `sanitize_link.py` | PlatformIO build script of the env `native-asan` (not a host tool): links the ASan/UBSan runtime with the same `-fsanitize` flags as the compile (AF36). |
| `embed_www.py` | PlatformIO build script: embeds `data/www` into the firmware (`app.js`, `style.css` gzip with ETag, `index.html` for the per-boot token) as `$BUILD_DIR/www/www_assets.h`; stops the build on `innerHTML`, `outerHTML`, `insertAdjacentHTML`, `document.write`, `eval(` or `new Function` (W-S7, W-S8, AF34). |

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
python tools/rest_example.py cyd-hf-1a2b.local --password      # asks for the web password (8 to 64 characters)
python tools/lxi_example.py --discover                          # VXI-11 broadcast, *IDN? of every device
python tools/lxi_example.py --address 192.168.1.50 --command "FREQ?"
python tools/lxi_example.py --address 192.168.1.50 --pyvisa     # TCPIP::<ip>::INSTR, needs pyvisa pyvisa-py

python tools/device_check.py --usb COM5                          # settings only, outputs stay off
python tools/device_check.py --tcp 192.168.1.50 --repeat 1000    # L6: 1000 x *IDN? timing
python tools/device_check.py --usb COM5 --outputs --json > report.json
```

`device_check.py` prints one `PASS` / `FAIL` / `SKIP` line per check and a summary. Exit code 0 = no FAIL,
1 = at least one FAIL, 2 = no connection. `--json` writes the report to stdout (text lines go to stderr).
The expected values (IDN pattern, limits, error texts, round-trip table, rejected inputs with their error
codes) are in the tables at the top of the script; adjust them when firmware or manual change.

**Web password** (`rest_example.py`, `web_check.py`): `--password` without a value asks for it without
echo (`getpass`); without `--password` the tools take the environment variable `CYD_WEB_PASSWORD`, if set.
`--password <value>` still works but prints a warning: the value lands in the shell history and in the
process list of the machine. The mock's own `--password` (below) is the password of the simulated
device, a test value.

### Offline try-out

```bash
python tools/fake_scpi_device.py --port 5025        # terminal 1
python tools/device_check.py --tcp 127.0.0.1:5025   # terminal 2
```

The fake does not implement `*RST`, so `--allow-rst` reports one FAIL against it; `OUTP:REM?` always answers
`ALLOWED`, so the `-221` check is skipped. Everything else passes against it.

VXI-11 without the device (clients always ask the portmapper on port 111; on Linux binding it needs root):

```bash
python tools/fake_scpi_device.py --vxi11                         # terminal 1
python tools/lxi_example.py --discover --broadcast 127.0.0.1     # terminal 2
python tools/lxi_example.py --address 127.0.0.1 --pyvisa
```

### Web interface without the device

`web_mock.py` serves the web interface from `data/www/` with the REST API, the WebSocket and the
request guard of ARBEITSPLAN 3.6 on a simulated device (standard library only):

```bash
python tools/web_mock.py --port 8080                 # then open http://127.0.0.1:8080/
python tools/web_mock.py --password secret123 --demo # with login and changing values
python tools/rest_example.py 127.0.0.1:8080          # the REST example against the mock
python tools/web_check.py 127.0.0.1:8080             # the contract check against the mock
python tools/web_check.py cyd-hf-1a2b.local --password    # ... against the device, asks for the password
```

The mock gives the firmware's answers (S55); where they differ, the firmware is the reference.

Further flags: `--local-only`, `--open-ap`, `--hw`, `--no-ws`, `--confirm-timeout`, `--token` (`--login-all` has no effect any more: with `--password` the state, token and `/ws` always need the login, D25)
(`--help` explains them).

## CI

The job `Tool checks` of `ci.yml` (AF34) runs `size_report.py --self-test` and `hostport.py`, starts `web_mock.py` on free ports (a port in use is an error: `allow_reuse_address` is off) without and with a password, runs `contract_check.py`, `web_check.py` and a short `slowloris.py` against them, and `device_check.py --tcp` (also `--profile L22`) and `counter_log.py` against `fake_scpi_device.py`.
