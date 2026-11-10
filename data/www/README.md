# Web user interface (WEB-HMI)

Static files the firmware embeds and serves (S55: `tools/embed_www.py` gzips `app.js` and `style.css` into the image at build time, `index.html` gets the per-boot token at runtime; no LittleFS). No framework, no external resources, English only (W-E11).

| File | Bytes | gzip -9 | Content |
|---|---:|---:|---|
| `index.html` | 3 693 | 1 475 | Skeleton: header, license footer, dialogs (help, login, confirm), `<meta name="cyd-token" content="{{TOKEN}}">` |
| `app.js` | 40 197 | 13 675 | Everything else: areas, rendering, REST, WebSocket/polling, keyboard |
| `style.css` | 9 140 | 3 069 | Dark theme in the colors of `include/palette.h`, responsive layout |
| total | 53 030 | 18 219 | |

This README is not embedded and not served.

## Rules the files keep

- CSP (W-S7): no inline script, no inline event handler, no `style="..."`, no `eval`/`new Function`. The only script is `<script src="app.js" defer>`.
- W-S8: text from the device (SSIDs, names, errors) goes into the page only through `textContent` and DOM methods, never `innerHTML`.
- Every POST sends `Content-Type: application/json` and `X-CYD-Token: <token from the meta tag>`. Writes are spaced by at least 60 ms (≤ 20/s, W-S11); steps that come faster than the device answers (held `+`, key repeat, mouse wheel) are merged into one request with the latest target.
- Buttons and fields are at least 44 CSS px; `touch-action: manipulation`.
- The footer (G11) is static text in `index.html`: `CYD HF Generator · GPL-3.0-or-later, no warranty · Source and licenses`, the two links (44 px high) open the repository and `THIRD_PARTY_NOTICES.md` on GitHub in a new tab (`rel="noopener noreferrer"`). The device display shows no license line.

## Areas

- **Header:** device name and version, connection (`Live` = WebSocket, `Polling`, `Offline`), save state (`Saved`, `Saving...`, `Save failed`, `Not saved`), `REMOTE <source>`, SI5351 problem, `Identify`, `Save`, `Log out` (only while a web password is set and this browser is logged in), `?`, and the links MAIN, CH1, CH2, CH3, CONFIG, WiFi. It stays at the top on every width (on a phone as well, like the output bar at the bottom); anchors stop below it (`--toph` follows its height). On a phone `MAIN` scrolls to the top, the cards are the output bar.
- **MAIN:** three cards as on the device: state (`ON`, `OFF`, `! ERR`, `WAIT`, `! NO HW`), frequency (`~` in Integer mode from 1 Hz deviation, below 100 MHz), mode line (`IQ master` / `IQ master of CH3`; CH3: `CH1 / 90°` with the reason for `WAIT`), switch. On a phone (< 700 px) the cards form a fixed output bar at the bottom and the areas are stacked.
- **CH1, CH2:** switch and state, frequency digits (the digit of the step in the channel color; click = step, wheel = change that digit, click on the unit = input field), `Out` line, mode line with `IQ master (90°)` and the range hint, step chips `1M` … `1`, `−`/`+` with hold-to-repeat as on the device, frequency field.
- **CH3 IQ:** switch, state, frequency, status line (`Off  |  follows CHx`, `Out …`, `Master CHx is off: click to switch on` as a button, `Waiting: CHx below 4.730 MHz`, …), master and phase chips, IQ range note, field for the master frequency.
- **CONFIG:** `Drive all`, `Off state` (`High` asks first, as on the device), `Tuning`, `Spread CH1` (locked as `Off (Integer)`), `Corr.` (field in ppm, step `0.01`/`0.1`/`1`, `−`/`+`), `Backlight` (`−`/`+` in 10 %), `Remote outputs` read-only, info (firmware, SI5351, device name).
- **WiFi:** status; form with mode, device name, two networks (password fields write-only, never pre-filled, empty = keep), `Delete`, scan list (≤ 15, `→ 1`/`→ 2` fills a slot), static IP, access point (`Open (not recommended)`, new password). `Apply` sends only what changed; then the countdown "Confirm within N s, otherwise the previous settings return" with `Confirm` and the address to open if the page loses the connection.

Live updates: a changed value is outlined briefly; a field with focus is never overwritten. While disconnected all controls are disabled and gray.

## Keyboard

| Key | Action |
|---|---|
| `↑` `↓`, `+` `−` (also keypad) | frequency of the selected channel ± step (CH3: its master) |
| `←` `→` | larger / smaller step |
| `PgUp` `PgDn` | frequency × 10 / ÷ 10 (refused outside 8 kHz – 160 MHz) |
| wheel over a digit | that digit ± 1 |
| `1` `2` `3` | select CH1/CH2/CH3 (only without a focused field) |
| `F`, `Enter` | focus the frequency field |
| `Enter` / `Esc` in the field | apply / discard |
| `O` | output of the selected channel on/off (not `Space`) |
| `?` | help |

## Trying it: `tools/web_mock.py`

Python 3, standard library only. Serves this directory with `{{TOKEN}}` replaced, the REST API with an in-memory state and a small RFC 6455 push WebSocket.

```bash
python tools/web_mock.py                       # prints http://127.0.0.1:8080/
python tools/web_mock.py --host 0.0.0.0        # for a phone in the LAN
python tools/web_mock.py --password secret123  # login for writes and WiFi (state stays public)
python tools/web_mock.py --password secret123 --login-all   # state and /ws need the login, too
python tools/web_mock.py --local-only          # Remote outputs: local only -> switch-on 403
python tools/web_mock.py --open-ap             # client on the open AP -> password fields 403
python tools/web_mock.py --hw NFO              # no SI5351: NO HW
python tools/web_mock.py --no-ws               # UI falls back to polling
python tools/web_mock.py --demo                # CH2 changes every 4 s ("someone at the touch screen")
python tools/web_mock.py --confirm-timeout 20  # shorter network fallback
```

It simulates the actual frequency per tuning mode (Integer on the 25 MHz × a / divider grid, spread as mean), CH3 `WAIT` outside the IQ range (narrower with spread on master CH1), autosave after 5 s, `REMOTE WEB` for 10 s after a write, a scan list with hostile SSIDs, the two-stage network change with fallback, login with lockout, 3 WebSocket clients and the "Too many clients (max. 3)" page.

## API contract as implemented

State (`GET /api/state`, every WebSocket text message, body of every successful POST except `/api/wifi*`, `/api/login`):

```json
{"rev":123,"hw":"CONN","save":"saved","remote":null,
 "ch":[{"n":1,"enabled":true,"frequency":10000000,"actual":10000000.0,"status":"ON","modes":["SSC -0.5%"]},
       {"n":2,"enabled":false,"frequency":14000000,"actual":14000000.0,"status":"OFF","modes":[]}],
 "ch3":{"enabled":false,"master":1,"phase":90,"status":"OFF","frequency":10000000,"actual":10000000.0,"iqMin":4730000,"iqMax":100000000},
 "config":{"drive":4,"tuning":"fractional","spread":"off","offState":"low","correction":0,"step":1000,"backlight":80,"remoteOutputs":"allowed"},
 "wifi":{"mode":"client","connected":true,"ssid":"Lab","rssi":-60,"ip":"192.168.1.50","name":"cyd-hf-1a2b",
         "ap":{"active":false,"ssid":"CYD-HF-1A2B","open":false,"stations":0},"confirmWithin":0},
 "device":{"version":"1.4.0","passwordSet":false}}
```

Values: `hw` CONN | NFO | BLOC | UNKN; `save` saved | pending | failed | unavailable; `remote` null | usb | web | scpi; status ON | OFF | ERR | WAIT | NOHW; `correction` in 0.01 ppm (±5000); `tuning` fractional | lownoise | integer; `spread` off | down05 | down15 | center05; `offState` low | high | hiz; `remoteOutputs` allowed | local; `wifi.mode` off | client | ap.

How the UI reads it:
- `modes` holds only the CONFIG entries of the device mode line, in device order (`OFF=High`/`OFF=High-Z`, `SSC -0.5%`/`SSC -1.5%`/`SSC +/-0.5%`, `Low noise`/`Integer`, `2mA`/`6mA`/`8mA`). The UI adds `IQ master …` and the range hint itself and shows the line orange when it contains `OFF=High`. CH3 shows the modes of its master.
- `actual` is what the `Out` line shows: with spread on CH1 (and CH3 with master CH1) the mean frequency, like `FREQ:ACT?`; 0 = no output (`Out -`). `ch3.actual` is 0 unless CH3 runs.
- `ch3.iqMin`/`iqMax` are the effective limits (narrower with spread); the reason for `WAIT` is derived: master off → `master off`, master below `iqMin` → `below`, else `above`.

| Request | Body | Success |
|---|---|---|
| `POST /api/channel/<1\|2>` | subset of `{"enabled":bool,"frequency":int,"frequencyText":str}`, at most one of the two frequency keys | 200 state |
| `POST /api/ch3` | subset of `{"enabled":bool,"master":1\|2,"phase":0\|90\|180\|270}` | 200 state |
| `POST /api/config` | subset of `drive`, `tuning`, `spread`, `offState`, `correction`, `step`, `backlight` | 200 state |
| `POST /api/save`, `POST /api/identify` | `{}` | 200 state |
| `POST /api/login` | `{"password":str}` | 204 + `Set-Cookie` (HttpOnly, SameSite=Strict, 12 h) |
| `POST /api/logout` | `{}` | 204 + `Set-Cookie: cyd_sid=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict`; ends the session of the cookie; needs no session (token and JSON as every POST) |
| `GET /api/wifi` | | `{"mode","networks":[{"ssid","key":"set\|open\|none"}×2],"ap":{"ssid","open","passwordSet"},"name","staticIp":{"enabled","ip","mask","gateway","dns"}}` |
| `POST /api/wifi` | subset of `mode`, `networks:[{slot,ssid,password}]`, `delete:[slot]`, `name`, `staticIp:{…}`, `ap:{open,password}` | 202 `{"confirmWithin":120}`; mode `off` is final: `{"confirmWithin":0}` |
| `POST /api/wifi/confirm` | `{}` | 204 |
| `GET /api/wifi/scan` | | `{"state":"running\|done\|failed","networks":[{"ssid","rssi","secure"}]}` |
| `GET /api/token` | | `{"token":"…"}` (same-origin only, for scripts) |
| WebSocket `/ws?t=<token>` | push only | state on connect and on every change |

What the UI sends for networks: an entry for a slot only when its SSID changed or a password was typed; `password` only when typed, or `""` for a new SSID without password (open network, the UI asks first). A cleared SSID or `Delete` → `delete`. `staticIp` is sent complete when any of its fields changed.

Errors (mock = the expected device answers): 400 `{"error":"-104,Data type error"}` (wrong JSON type, `"4"`, `4.5`, `4.0`, `true` for an int, invalid JSON), `-108,Parameter not allowed` (unknown key), `-221,Settings conflict` (`frequency` and `frequencyText` together, spread in Integer, static IP = gateway / outside the subnet / network or broadcast address, network change while one waits), `-222,Data out of range` (frequency, correction, backlight outside 10–100), `-224,Illegal parameter value` (values outside an enumeration, `frequencyText` that does not parse, backlight not in 10 % steps, bad device name, password not 8–63); 401 login required, 403 token / origin / `Sec-Fetch-Site: cross-site` / `-221,Remote outputs: local only` / `remoteOutputs` write / password fields over the open AP, 404, 405 for OPTIONS, 413 body > 1 KB, 415 not JSON or chunked, 421 foreign `Host`, 429 `-200,Too many commands` (> 20 writes/s, scan within 10 s) and login lockout (5 failures → 30 s, doubling, `retryAfter`), 503 4th WebSocket. The UI shows the text after the comma.

`frequencyText` in the mock: digits with blanks between groups, one `.` or `,`, optional unit `Hz`, `k`/`kHz`, `M`/`MHz` (any case); a decimal needs `k` or `M`; rounded to whole Hz; no sign, exponent or hex.

Scan: the first GET starts a scan (`running`), the GET after it finished returns `done` once; another start within 10 s of the previous start answers 429. The UI polls once per second while `running`.

## Settled in S55 (firmware = reference, the mock follows it)

- `wifi.confirmWithin` (seconds left, 0 = nothing pending) is part of the state; a page opened at the new address shows `Confirm`.
- Mode `off` over the web is final: applied after the answer and saved, `confirmWithin` 0, the UI says WiFi switches off.
- `device.passwordSet` tells whether a web password is set; it is set at the device only (no REST call, WIFI > Device > `Web pw.`). Setting, changing or removing it ends every session; the UI then hides `Log out` until the next login. Without one, a network change needs `Keep` at the touch screen as well: a web `Confirm` before that answers `-221,Confirm at the device as well` and counts once the touch confirms.
- Stale token after a restart: 403 `-200,Invalid token: reload the page`.
- Invalid JSON and duplicate keys `-102`; wrong type `-104`; unknown key and `frequency` with `frequencyText` `-108`; `{}` where a key is needed `-109`; out of range `-222` (also `frequencyText` outside 8 kHz–160 MHz; text that is no frequency `-224`); values outside a list (master, phase, drive, step, enums, slot) `-224`; backlight any integer 10–100.
- `/api/state` and `/ws` are readable without login also when a password is set (they hold no secrets, MAC or commit).
- Without connection `wifi.ssid`/`ip` are `""`, `rssi` 0; with the AP `ip` is the AP address.
- No REST call for `Defaults` / `Factory`, touch calibration, screen lock, AP `Regenerate` and the web password; the UI says these are set at the device.
- `PgUp`/`PgDn` are read as frequency × 10 / ÷ 10 (ARBEITSPLAN 3.6 "× 10 / ÷ 10"); `←`/`→` already step the step size by decades.

## Settled in S56

- `POST /api/logout` with `{}`: 204 and an expired cookie, also without a valid session (an expired one must not ask for the password first); other keys `-108`. The UI shows `Log out` once a request that needs the session succeeded (login, WiFi settings, a write) and hides it on 401, after the logout and when `device.passwordSet` changes; after the logout it clears the WiFi form (it came with the session).
- The UI uses `S.ch3.master` only as 1 or 2 (CodeQL, G4).
