# Remote Control API – CYD HF Generator

Reference for controlling the generator from a program: setup, connections and every command over USB and the network. The behavior described here is the same as in the user manual ([section 15](../USER_MANUAL_EN.md#15-remote-control-over-usb-and-network-scpi) and [section 16](../USER_MANUAL_EN.md#16-web-interface)); the complete REST contract with all status codes is in [data/www/README.md](../data/www/README.md).

## 1. Interfaces at a Glance

| Interface | Transport | Address | Protocol | Login |
|---|---|---|---|---|
| SCPI over USB | USB serial (CH340), 115 200 8N1 | `COMx` / `/dev/ttyUSB0` | SCPI lines, LF | none (physical access) |
| SCPI over TCP | WiFi, TCP 5025 | `<ip>` or `<name>.local` | SCPI lines, LF | none |
| VXI-11 (LXI) | WiFi, portmapper UDP/TCP 111, core TCP 9009 | `TCPIP::<ip>::INSTR` | ONC RPC, SCPI inside | none |
| REST | WiFi, HTTP port 80 | `http://<ip>/api/...` | JSON | web password, if set |
| WebSocket | WiFi, HTTP port 80 | `ws://<ip>/ws?t=<token>` | JSON push (state) | as REST reads |
| LXI identification | WiFi, HTTP port 80 | `http://<ip>/lxi/identification` | XML | none |

- All interfaces act on the same state as the touch screen. The last change wins; every change appears on the display at once, and after a remote change the status bar shows `REMOTE <source>` (`USB`, `SCPI`, `WEB`) for 10 s.
- Every source may send at most **20 setting commands per second**; further ones are refused (SCPI `-200,"Execution error;Too many commands"`, REST 429).
- **Remote outputs:** the device setting CONFIG → `Net` → `Remote out.` = `local only` lets every remote interface set everything and switch outputs off, but not switch them on and not select `Off state` `High`. It can only be changed at the device.
- Only at the device: defaults and factory reset, touch calibration, screen lock, access point password, web password, `Remote out.`.

## 2. Setup

### 2.1 USB

1. Connect the board with a USB data cable (CYD: micro USB, CYD2USB: USB-C). The USB-serial chip is a CH340; Windows and Linux usually bring the driver, otherwise install the CH340 driver of the chip vendor.
2. Find the port: Windows Device Manager → "Ports (COM & LPT)" → `USB-SERIAL CH340 (COMx)`; Linux `/dev/ttyUSB0` (user in group `dialout`); macOS `/dev/cu.wchusbserial*`.
3. Open the port with **115 200 baud, 8N1, DTR and RTS off**. With DTR/RTS on, the auto-reset circuit of the board restarts it when the port opens.
4. Send a line, e.g. `*IDN?` + LF. The first line starts an **SCPI session**: from then on no log lines are written to USB, so answers are not mixed with log text. They go to a buffer of 32 lines that `SYST:LOG?` reads. After 10 minutes without a line the session ends and the log runs over USB again.

### 2.2 WiFi

The network interfaces need WiFi (default `Off`). At the device:

**Client in an existing network (recommended):**

1. CONFIG → tab `Net` → row `WiFi`: choose `Client`.
2. `Setup` (WIFI page) → network `1` → `Change` → choose the network from the scan list (`▲`/`▼` page, `Other...` for a hidden network) → type the password (8–63 characters or a 64-digit hex key) → ✓.
3. The status line of the WIFI page shows `Connected <network>` with the address and `<name>.local`.
4. Optional, WIFI → `Device`: device name (`cyd-hf-<4 digits>` by default, reachable as `<name>.local`), static IP (`Static`, address, mask, gateway, DNS) and the web password.

**Own access point (no router):**

1. CONFIG → `Net` → `WiFi`: choose `Access point`.
2. WIFI → `Show` on the AP card: network name `CYD-HF-<4 digits>`, password and QR code.
3. Connect the PC or phone to that network; the device has the address **`192.168.4.1`**.

In `Client` mode the device opens the same access point as a fallback when it finds no stored network for 60 s.

**Security notes:**

- SCPI over TCP and VXI-11 have **no login** (as usual for lab instruments). Anyone who reaches the device can run every command. Use a trusted network (a separate guest or IoT WiFi) or the own access point with password, and `Remote out.` = `local only` against switching outputs on from the network.
- The **web password** protects only the web interface and REST.
- Over an **open** access point (without password) SCPI over TCP and VXI-11 are blocked and no password can be sent (web login refused with 403).
- No HTTPS; the web interface and REST are plain HTTP.

### 2.3 Finding the Device

| Method | Finds |
|---|---|
| mDNS name | `<name>.local` (e.g. `cyd-hf-1a2b.local`) |
| mDNS services | `_scpi-raw._tcp` (5025), `_vxi-11._tcp` (111), `_lxi._tcp` (80, TXT `Manufacturer=DM8SB`, `Model=CYD HF Generator`), `_http._tcp` (80) |
| VXI-11 broadcast | `lxi discover`, PyVISA `list_resources()`, `tools/lxi_example.py --discover` |
| Device | WIFI page status line, CONFIG → `Info` (`WiFi <network> <ip> <rssi>`) |

## 3. SCPI

The same command set and the same line rules apply over USB, TCP 5025 and VXI-11.

### 3.1 Line Format

- A line ends with LF (`\n`); a CR before it is ignored. Answers end with LF.
- At most 256 characters and 16 commands per line, separated by `;`. Only printable ASCII and TAB. A line that breaks these rules is discarded completely: −363 (too long), −101 (invalid character), −223 (too many commands).
- Short form in capitals, any case: `FREQ`, `freq`, `FREQuency`. `[ ]` is optional, `<n>` is the channel 1–3 (1 if omitted). Queries end with `?`.
- `;:` starts again at the root: `FREQ 7.1MHZ;:OUTP1 ON`. Answers of several queries come in one line separated by `;`: `FREQ?;:OUTP1?` → `7100000;1`.
- Commands run at once; there is no overlapped execution. A refused command changes nothing; the next command after `;` still runs.

### 3.2 Numbers

- Whole Hz (`7100000`), decimal with or without unit `HZ`, `KHZ`, `MHZ` (`7.1 MHZ`, `455khz`), exponent (`7.1E6`); rounded to 1 Hz.
- `MIN`, `MAX`, `DEF` set the limit or the default.
- Nothing is clipped: out of range −222; `NAN`, `INF`, hex, unknown units, values not allowed −224; strings −104.

### 3.3 Command Reference

| Command | Query answer | Meaning |
|---|---|---|
| `*IDN?` | `CYD,HF-Generator,<MAC>,<version>` | Identification |
| `*RST` | – | All outputs off, defaults in RAM (correction and `Remote out.` kept), not saved until `SYST:SAVE` or the next change |
| `*CLS` | – | Clear the error queue and event status |
| `*ESE <n>`, `*ESE?` | `<n>` | Event status enable register |
| `*ESR?` | `<n>` | Event status register (read clears) |
| `*SRE <n>`, `*SRE?` | `<n>` | Service request enable register |
| `*STB?` | `<n>` | Status byte |
| `*OPC`, `*OPC?`, `*WAI` | `1` | Operation complete (commands run at once) |
| `*TST?` | `0` / `1` | Self test: `1` = no SI5351 connected |
| `OUTPut<n>[:STATe] ON\|OFF\|1\|0` | `1` / `0` | Output on/off (CH3: request like its switch) |
| `OUTPut<n>:CONDition?` | `ON`, `OFF`, `ERR`, `WAIT`, `NOHW` | Actual state as shown on the display |
| `[SOURce<n>:]FREQuency[:CW] <f>` | Hz | Frequency CH1/CH2, 8 kHz – 160 MHz; on CH3 query only (its master's frequency), setting gives −221 `CH3 follows its master` |
| `[SOURce<n>:]FREQuency:ACTual?` | Hz, 0.1 Hz resolution | Frequency the SI5351 actually produces (mean with spread spectrum) |
| `SOURce3:IQ:MASTer 1\|2` | `1` / `2` | Master channel of CH3 |
| `SOURce3:PHASe 0\|90\|180\|270` | degrees | Phase of CH3 against its master; unit `DEG` allowed |
| `SOURce3:IQ:RANGe?` | `<min>,<max>` Hz | IQ range of the master (4.730–100 MHz, narrower with spread spectrum) |
| `OUTPut:DRIVe 2\|4\|6\|8` | mA | Drive strength of all outputs |
| `OUTPut:OFFState LOW\|HIGH\|HIZ` | `LOW` / `HIGH` / `HIZ` | Level of switched-off outputs; `HIGH` refused under `local only` (−221) |
| `OUTPut:REMote?` | `ALLOWED` / `LOCAL` | `Remote out.` (read only) |
| `[SOURce1:]SSC OFF\|DOWN05\|DOWN15\|CENT05` | | Spread spectrum on CH1 (PLL A): off, −0.5 %, −1.5 %, ±0.5 %; in `Integer` tuning only `OFF` (−221 `Not in Integer tuning`) |
| `SYSTem:TUNing FRACtional\|LNOise\|INTeger` | `FRAC` / `LNO` / `INT` | Divider scheme (manual section 8, `Tuning`) |
| `SYSTem:CORRection <ppm>` | ppm, 2 decimals | Crystal correction ±50 ppm, rounded to 0.01 ppm; unit `PPM` allowed |
| `SYSTem:STEP <Hz>` | Hz | Step of `+`/`−` at the device: 1 Hz … 1 MHz in powers of ten; units allowed |
| `SYSTem:BACKlight <%>` | % | Display brightness 10–100 %, rounded to 10 % (55 → 60); unit `PCT` allowed |
| `SYSTem:SI5351?` | `CONN` / `NFO` / `BLOC` / `UNKN` | SI5351 link: connected, not found, line held low, foreign device |
| `SYSTem:HEAP?` | `<free>,<minimum>,<largest block>` | Memory in bytes (at most 0.1 s old) |
| `SYSTem:IDENtify` | – | Backlight and RGB LED blink for 5 s |
| `SYSTem:LOG?` | `"<line>"` | Oldest buffered log line, `""` when empty; USB only (TCP and VXI-11 always `""`) |
| `SYSTem:SAVE` | – | Save the changed settings at once; at most once per 10 s per interface, a further request within 10 s is merged silently and saved 10 s after the previous save |
| `SYSTem:ERRor[:NEXT]?` | `<code>,"<text>"` | Next entry of the error queue (16 entries), `0,"No error"` when empty |
| `SYSTem:ERRor:COUNt?` | `<n>` | Number of queued errors |
| `SYSTem:VERSion?` | `1999.0` | SCPI version |

### 3.4 Errors

| Code | Meaning |
|---|---|
| −101 | Invalid character (e.g. `FREQ 7MHZ 8MHZ`, `0x10`) |
| −104 | Data type error (string where a number is expected) |
| −108 | Parameter not allowed (one too many: `FREQ 7MHZ,1`) |
| −109 | Missing parameter (`FREQ`) |
| −113 | Undefined header (unknown command) |
| −114 | Header suffix out of range (`OUTP4`, `SOUR2:PHAS`) |
| −200 | `Execution error;Too many commands` (> 20/s) or `Execution error;Busy` (queue full, send again) |
| −221 | Settings conflict: `CH3 follows its master`, `Not in Integer tuning`, `Remote outputs: local only` |
| −222 | Data out of range |
| −223 | Too many commands in one line |
| −224 | Illegal parameter value |
| −225 | Out of memory (VXI-11: answers of one line > 2 KB) |
| −240 | `Hardware error;SI5351` (the SI5351 did not take the setting) |
| −363 | Line too long |
| −410 | Query interrupted (VXI-11: new command before the previous answer was read) |

Errors caused by remote commands open no dialog on the display; they are in the error queue and the log.

### 3.5 Saving

Settings changed over SCPI are saved 30 s after the last change (touch: 5 s). `SYST:SAVE` saves at once. `*RST` changes RAM only.

### 3.6 Per Transport

**USB**

- One session per port; see 2.1 for DTR/RTS and the log buffer.
- Error queue and status registers belong to the USB session.

**TCP 5025**

- One connection at a time; a second one is closed at once without an answer.
- The connection has its own error queue; it starts empty with every connection (`*ESE`, `*SRE` 0).
- 10 min without a line closes the connection; a vanished peer is noticed after about 90 s (keepalive).
- A line that starts like an HTTP request closes the connection.
- If the peer does not read its answers, the device stops reading and closes the connection when the buffer is full.
- VISA resource: `TCPIP0::<ip>::5025::SOCKET` with read and write termination `\n`.

**VXI-11**

- Portmapper on UDP and TCP 111 (answers the broadcast search; two TCP connections at a time), core channel fixed on TCP 9009. Device name `inst0`.
- One connection with one link at a time; a second client is closed at once. 10 min idle closes it, keepalive as for TCP.
- A command ends with LF or with the END flag of the write call. A read returns the answer of a query up to its LF; with no answer waiting the device reports a timeout at once (VXI-11 error 15).
- `read_stb()` returns the status byte; bit 4 (MAV, 16) is set while an answer waits. `clear()` (device clear) discards half lines and unread answers; the error queue stays (`*CLS` clears it).
- Own error queue per link, starts empty with every link.
- Not supported: HiSLIP, abort channel (port 0), interrupt channel and SRQ, `trigger()` and `device_docmd` (VXI-11 error 8), several links. `lock()`/`unlock()` and `remote`/`local` are accepted but do nothing; a lock is no access control.

### 3.7 Examples

Python with pyserial (USB):

```python
import serial

gen = serial.Serial()
gen.port = "COM5"          # Linux: /dev/ttyUSB0
gen.baudrate = 115200
gen.timeout = 2
gen.dtr = False            # before open(): no restart
gen.rts = False
gen.open()

def query(cmd):
    gen.write((cmd + "\n").encode())
    return gen.readline().decode().strip()

print(query("*IDN?"))
gen.write(b"FREQ 7.1 MHZ\n")
gen.write(b"OUTP1 ON\n")
print(query("FREQ:ACT?"))
print(query("SYST:ERR?"))
```

Python sockets (TCP 5025):

```python
import socket

with socket.create_connection(("192.168.1.50", 5025), timeout=5) as s:
    f = s.makefile("rw", newline="\n")
    f.write("*IDN?\n"); f.flush()
    print(f.readline().strip())
    f.write("SOUR2:FREQ 14.2MHZ;:OUTP2 ON\n"); f.flush()
    f.write("SYST:ERR?\n"); f.flush()
    print(f.readline().strip())
```

PyVISA with pyvisa-py (`pip install pyvisa pyvisa-py`):

```python
import pyvisa

rm = pyvisa.ResourceManager("@py")
gen = rm.open_resource("TCPIP::192.168.1.50::INSTR", open_timeout=5000)   # VXI-11
# raw TCP: rm.open_resource("TCPIP::192.168.1.50::5025::SOCKET", open_timeout=5000)
gen.read_termination = "\n"
gen.write_termination = "\n"
print(gen.query("*IDN?"))
gen.write("SOUR3:IQ:MAST 1;:SOUR3:PHAS 90;:OUTP3 ON")
print(gen.query("OUTP3:COND?"))
gen.close()
```

`open_timeout` is needed: pyvisa-py otherwise waits only about 0.1 s for the TCP connect, and over WiFi the device may need 0.2–0.4 s.

lxi-tools:

```bash
lxi discover                               # VXI-11 broadcast
lxi discover -m                            # mDNS
lxi scpi -a 192.168.1.50 "*IDN?"           # VXI-11
lxi scpi -a 192.168.1.50 -r "FREQ?"        # raw TCP 5025
lxi scpi -a 192.168.1.50 -r -i             # interactive
```

python-vxi11 (`pip install python-vxi11`; from Python 3.13 also `standard-xdrlib`):

```python
import vxi11
gen = vxi11.Instrument("192.168.1.50")
print(gen.ask("*IDN?"))
gen.close()
```

## 4. REST and WebSocket

### 4.1 Requests

- Base URL `http://<ip>` or `http://<name>.local`; in the own access point `http://192.168.4.1`. The `Host` header must be the device address, `<name>` or `<name>.local` (else 421).
- Every `POST` needs `Content-Type: application/json`, a `Content-Length` and the header **`X-CYD-Token: <token>`**. The token is new with every restart; read it from `<meta name="cyd-token" content="…">` of `GET /` (or `GET /api/token` from a same-origin page). A stale token gives 403 `Invalid token: reload the page`.
- Limits: body ≤ 1 KB, path ≤ 256 bytes, headers ≤ 2 KB, at most 4 HTTP connections, 20 writes/s.
- Settings written over REST are saved 5 s after the last change; `POST /api/save` saves the changed values at once, at most once per 10 s (a further request within 10 s answers 200 and is saved 10 s after the previous save).

### 4.2 Login

Only when a web password is set at the device:

- `POST /api/login` with `{"password":"…"}` → 204 and a session cookie (`HttpOnly`, `SameSite=Strict`, 12 h). Send the cookie with every following request.
- Without a valid session every `POST` (except login/logout) and `GET /api/wifi*` answer 401 `Login required`. `GET /api/state` and the WebSocket stay readable without login.
- 5 wrong passwords lock the login for 30 s, every further lock doubles (up to 16 min): 429 with `retryAfter`.
- `POST /api/logout` with `{}` → 204, ends the session.
- Over an open access point any body with a password field is refused (403).

### 4.3 State

`GET /api/state` (also every WebSocket message and the body of every successful write except `/api/wifi*` and `/api/login`):

```json
{"rev":123,"hw":"CONN","save":"saved","remote":null,
 "ch":[{"n":1,"enabled":true,"frequency":10000000,"actual":10000000.0,"status":"ON","modes":["SSC -0.5%"]},
       {"n":2,"enabled":false,"frequency":14000000,"actual":14000000.0,"status":"OFF","modes":[]}],
 "ch3":{"enabled":false,"master":1,"phase":90,"status":"OFF","frequency":10000000,"actual":10000000.0,"iqMin":4730000,"iqMax":100000000},
 "config":{"drive":4,"tuning":"fractional","spread":"off","offState":"low","correction":0,"step":1000,"backlight":80,"remoteOutputs":"allowed"},
 "wifi":{"mode":"client","connected":true,"ssid":"Lab","rssi":-60,"ip":"192.168.1.50","name":"cyd-hf-1a2b",
         "ap":{"active":false,"ssid":"CYD-HF-1A2B","open":false,"stations":0},"confirmWithin":0},
 "device":{"version":"2.0.0","passwordSet":false}}
```

| Field | Values |
|---|---|
| `rev` | revision, increases with every change |
| `hw` | `CONN`, `NFO`, `BLOC`, `UNKN` (as `SYST:SI5351?`) |
| `save` | `saved`, `pending`, `failed`, `unavailable` |
| `remote` | `null`, `usb`, `web`, `scpi` (last remote source, 10 s) |
| `status` | `ON`, `OFF`, `ERR`, `WAIT`, `NOHW` |
| `frequency` / `actual` | Hz (requested / produced, mean with spread; 0 = no output) |
| `modes` | mode line entries of the device (`OFF=High`, `SSC -0.5%`, `Low noise`, `Integer`, `8mA`, …) |
| `correction` | 0.01 ppm (±5000) |
| `tuning` | `fractional`, `lownoise`, `integer` |
| `spread` | `off`, `down05`, `down15`, `center05` |
| `offState` | `low`, `high`, `hiz` |
| `remoteOutputs` | `allowed`, `local` |
| `wifi.mode` | `off`, `client`, `ap` |
| `confirmWithin` | seconds left to confirm a network change, 0 = none |

### 4.4 Endpoints

| Request | Body | Answer |
|---|---|---|
| `GET /api/state` | – | 200 state |
| `POST /api/channel/1`, `/api/channel/2` | subset of `{"enabled":bool,"frequency":int,"frequencyText":str}` (one frequency key at most; `frequencyText` like `"7.1M"`, `"455k"`, `"10,7 MHz"`) | 200 state |
| `POST /api/ch3` | subset of `{"enabled":bool,"master":1\|2,"phase":0\|90\|180\|270}` | 200 state |
| `POST /api/config` | subset of `drive` (2/4/6/8), `tuning`, `spread`, `offState`, `correction` (0.01 ppm), `step` (1…1000000, powers of ten), `backlight` (10–100, rounded to 10) | 200 state |
| `POST /api/save` | `{}` | 200 state |
| `POST /api/identify` | `{}` | 200 state (display and LED blink 5 s) |
| `POST /api/login` | `{"password":str}` | 204 + cookie |
| `POST /api/logout` | `{}` | 204 + expired cookie |
| `GET /api/wifi` | – | `{"mode","networks":[{"ssid","key":"set\|open\|none"}×2],"ap":{"ssid","open","passwordSet"},"name","staticIp":{"enabled","ip","mask","gateway","dns"}}` |
| `POST /api/wifi` | subset of `mode`, `networks:[{slot,ssid,password}]`, `delete:[slot]`, `name`, `staticIp:{…}`, `ap:{open,password}` | 202 `{"confirmWithin":120}` |
| `POST /api/wifi/confirm` | `{}` | 204 |
| `GET /api/wifi/scan` | – | `{"state":"running\|done\|failed","networks":[{"ssid","rssi","secure"}]}` (start at most every 10 s) |
| `GET /api/token` | – | `{"token":"…"}` (same-origin only) |
| `GET /lxi/identification` | – | LXI identification XML |
| WebSocket `/ws?t=<token>` | – | state on connect and on every change (push only, at most 3 clients) |

**Network changes** (`POST /api/wifi`) only stay after a confirmation, so nobody locks themselves out: the WiFi restarts with the new data, then `POST /api/wifi/confirm` must arrive within 120 s at the new address; without a web password the touch screen asks `Network change` as well (`Keep`). Otherwise the previous settings return. Only the confirmed state is saved (a power loss meanwhile brings back the previous settings), and while the change is open the touch screen changes no network settings; it offers `Revert` (`Web change pending`), also with a web password. `slot` is 0 or 1 (network 1 and 2); `password` is `""` (open network), 8–63 characters or 64 hex digits; passwords are write-only. `{"mode":"off"}` applies at once without confirmation and must come alone (any other key → 400 `-108`); WiFi can then only be switched on at the device.

### 4.5 Errors

Every error body is `{"error":"<code>,<text>"}` with the codes of SCPI. Main HTTP status codes:

| Status | Cause |
|---|---|
| 400 | `-102` invalid JSON or duplicate key, `-104` wrong JSON type, `-108` unknown key or not allowed combination, `-109` missing parameter, `-221` settings conflict, `-222` out of range, `-224` illegal value |
| 401 | `Login required`, `Wrong password` |
| 403 | stale token, foreign origin, `-221,Remote outputs: local only` (switching on, `offState` `high`), `remoteOutputs` written, password over an open AP |
| 404 | unknown path, `/api/channel/<n>` other than 1 and 2 |
| 413 / 414 / 431 | body, path or headers too large |
| 421 | `Host not allowed` |
| 429 | more than 20 writes/s, scan within 10 s, login lock (`retryAfter`) |
| 500 | `-240,SI5351 setup failed` (written, but the SI5351 refused it) |
| 503 | busy, low memory, too many connections or clients |

The complete list with every text is in [data/www/README.md](../data/www/README.md#errors).

### 4.6 Examples

curl (without web password):

```bash
IP=192.168.1.50
TOKEN=$(curl -s http://$IP/ | sed -n 's/.*name="cyd-token" content="\([^"]*\)".*/\1/p')
curl -s http://$IP/api/state
curl -s -X POST http://$IP/api/channel/1 \
     -H "Content-Type: application/json" -H "X-CYD-Token: $TOKEN" \
     -d '{"frequency":7100000,"enabled":true}'
curl -s -X POST http://$IP/api/config \
     -H "Content-Type: application/json" -H "X-CYD-Token: $TOKEN" \
     -d '{"tuning":"lownoise","drive":8}'
```

With a web password, log in first and keep the cookie:

```bash
curl -s -c jar.txt -X POST http://$IP/api/login \
     -H "Content-Type: application/json" -H "X-CYD-Token: $TOKEN" -d '{"password":"secret123"}'
curl -s -b jar.txt -X POST http://$IP/api/save \
     -H "Content-Type: application/json" -H "X-CYD-Token: $TOKEN" -d '{}'
```

Python: `tools/rest_example.py` (token, optional login, state, writes, errors).

## 5. Tools in the Repository

| Tool | Purpose |
|---|---|
| `tools/scpi_example.py` | SCPI over USB (`--usb COM5`), TCP (`--tcp <ip>`) or PyVISA (`--visa <resource>`) |
| `tools/lxi_example.py` | VXI-11 discovery (`--discover`), commands over VXI-11 or PyVISA |
| `tools/rest_example.py` | REST with token and optional login |
| `tools/device_check.py` | checks the SCPI command set on a device and restores it |
| `tools/fake_scpi_device.py` | stand-in device over TCP (`--vxi11` adds VXI-11) |
| `tools/web_mock.py` | web interface and REST without hardware |

Instructions: [tools/README.md](../tools/README.md).
