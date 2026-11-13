# Web user interface (WEB-HMI)

Static files the firmware embeds and serves (S55: `tools/embed_www.py` gzips `app.js` and `style.css` into the image at build time, `index.html` gets the per-boot token at runtime; no LittleFS). No framework, no external resources, English only (W-E11).

| File | Bytes | gzip -9 | Content |
|---|---:|---:|---|
| `index.html` | 3 812 | 1 541 | Skeleton: header, license footer, dialogs (help, login, confirm), `<meta name="cyd-token" content="{{TOKEN}}">` |
| `app.js` | 41 247 | 14 128 | Everything else: areas, rendering, REST, WebSocket/polling, keyboard |
| `style.css` | 9 230 | 3 134 | Dark theme in the colors of `include/palette.h`, responsive layout |
| total | 54 289 | 18 803 | |

This README is not embedded and not served.

## Rules the files keep

- CSP (W-S7): no inline script, no inline event handler, no `style="..."`, no `eval`/`new Function`. The only script is `<script src="app.js" defer>`.
- W-S8: text from the device (SSIDs, names, errors) goes into the page only through `textContent` and DOM methods, never `innerHTML`.
- Every POST sends `Content-Type: application/json` and `X-CYD-Token: <token from the meta tag>`. Writes are spaced by at least 60 ms (≤ 20/s, W-S11); steps that come faster than the device answers (held `+`, key repeat, mouse wheel) are merged into one request with the latest target.
- Buttons and fields are at least 44 CSS px; `touch-action: manipulation`.
- The footer (G11) is static text in `index.html`: `CYD HF Generator · GPL-3.0-or-later, no warranty · Source and licenses`, the two links (44 px high) open the repository and `THIRD_PARTY_NOTICES.md` on GitHub at the release tag of the firmware in a new tab (`rel="noopener noreferrer"`; `{{SRC_REF}}` is replaced at build time by `tools/embed_www.py`, E-39). The device display shows no license line.

## Areas

- **Header:** device name and version, connection (`Live` = WebSocket, `Polling`, `Offline`), save state (`Saved`, `Saving...`, `Save failed`, `Not saved`), `REMOTE <source>`, SI5351 problem, `Identify`, `Save`, `Log out` (only while a web password is set and this browser is logged in), `?`, and the links MAIN, CH1, CH2, CH3, CONFIG, WiFi. It stays at the top on every width (on a phone as well, like the output bar at the bottom); anchors stop below it (`--toph` follows its height). On a phone `MAIN` scrolls to the top, the cards are the output bar.
- **MAIN:** three cards as on the device: state (`ON`, `OFF`, `! ERR`, `WAIT`, `! NO HW`), frequency (`~` in Integer mode from 1 Hz deviation, below 100 MHz), mode line (`IQ master` / `IQ master of CH3`; CH3: `CH1 / 90°` with the reason for `WAIT`), switch. On a phone (< 700 px) the cards form a fixed output bar at the bottom and the areas are stacked.
- **CH1, CH2:** switch and state, frequency digits (the digit of the step in the channel color; click = step, wheel = change that digit, click on the unit = input field), `Out` line, mode line with `IQ master (90°)` and the range hint, step chips `1M` … `1`, `−`/`+` with hold-to-repeat as on the device, frequency field.
- **CH3 IQ:** switch, state, frequency, status line (`Off  |  follows CHx`, `Out …`, `Master CHx is off: click to switch on` as a button, `Waiting: CHx below 4.730 MHz`, …), master and phase chips, IQ range note, field for the master frequency.
- **CONFIG:** `Drive all`, `Off state` (`High` asks first, as on the device; under `remoteOutputs` `local` the device refuses it with 403, so the UI sends it without asking and shows the error), `Tuning`, `Spread CH1` (locked as `Off (Integer)`), `Corr.` (field in ppm, step `0.01`/`0.1`/`1`, `−`/`+`), `Backlight` (`−`/`+` in 10 %), `Remote outputs` read-only, info (firmware, SI5351, device name).
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
python tools/web_mock.py --local-only          # Remote outputs: local only -> switch-on and offState high 403
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
 "device":{"version":"2.0.0","passwordSet":false}}
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
| `POST /api/config` | subset of `drive`, `tuning`, `spread`, `offState`, `correction`, `step`, `backlight`; `offState` `high` under `remoteOutputs` `local` → 403 `-221,Remote outputs: local only` (D22) | 200 state |
| `POST /api/save`, `POST /api/identify` | `{}` | 200 state |
| `POST /api/login` | `{"password":str}` | 204 + `Set-Cookie` (HttpOnly, SameSite=Strict, 12 h); over an open AP 403 `-221,Passwords cannot be sent over the open access point` (W-S1, SEC-3), the UI does not send it there (`wifi.ap.active` and `open`) |
| `POST /api/logout` | `{}` | 204 + `Set-Cookie: cyd_sid=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict`; ends the session of the cookie; needs no session (token and JSON as every POST) |
| `GET /api/wifi` | | `{"mode","networks":[{"ssid","key":"set\|open\|none"}×2],"ap":{"ssid","open","passwordSet"},"name","staticIp":{"enabled","ip","mask","gateway","dns"}}` |
| `POST /api/wifi` | subset of `mode`, `networks:[{slot,ssid,password}]`, `delete:[slot]`, `name`, `staticIp:{…}`, `ap:{open,password}` | 202 `{"confirmWithin":120}`; mode `off` is final: `{"confirmWithin":0}`, and only `{"mode":"off"}` alone is taken (any other key → 400 `-108,Parameter not allowed`, D23) |
| `POST /api/wifi/confirm` | `{}` | 204 |
| `GET /api/wifi/scan` | | `{"state":"running\|done\|failed","networks":[{"ssid","rssi","secure"}]}` |
| `GET /api/token` | | `{"token":"…"}` (same-origin only, for scripts) |
| `GET /lxi/identification` | | LXI identification XML (`text/xml`, A-78): manufacturer, model, serial number as in `*IDN?`, firmware version, device name, address/mask/gateway of the connection, `TCPIP::<ip>::INSTR` and `TCPIP::<ip>::5025::SOCKET`; Host check as for the page, no Origin or token; readable without login, also when a web password is set (D33, stays so with `loginForReads`); not used by the UI |
| WebSocket `/ws?t=<token>` | push only | state on connect and on every change |

`slot` is 0 or 1 (= network 1 and 2 at the device); `password` is `""` (open network), a passphrase of 8–63 printable ASCII characters or exactly 64 hex digits (PMK, D31); the AP password 8–63 characters.

What the UI sends for networks: an entry for a slot only when its SSID changed or a password was typed; `password` only when typed, or `""` for a new SSID without password (open network, the UI asks first). A cleared SSID or `Delete` → `delete`. `staticIp` is sent complete when any of its fields changed.

### Errors

Every error body is `{"error":"<code>,<text>"}` (the login lock adds `"retryAfter":<s>`); the UI shows the text after the comma. The codes of a 400 come from the firmware (`remote/rest_json.cpp`, `remote/rest_wifi.cpp`, `core/app_controller.cpp`); the first problem in key order answers, and nothing is applied:

| Code | Cause |
|---|---|
| `-102,Syntax error` | invalid JSON, a duplicate key (`Duplicate key`) |
| `-104,Data type error` | wrong JSON type: `"4"`, `4.5`, `4.0` or `true` for an integer, a number for a string, an array for an object, a body that is no JSON object |
| `-108,Parameter not allowed` | unknown key; `frequency` and `frequencyText` together (`frequency or frequencyText, not both`); WiFi `mode` `off` with any other key (D23); `{}` bodies (`save`, `identify`, `logout`, `wifi/confirm`) with a key |
| `-109,Missing parameter` | `{}` where at least one key is needed |
| `-221,Settings conflict` | spread in `Integer` tuning (`Spread spectrum is not possible in Integer tuning`); static IP = gateway, outside the subnet, network or broadcast address; `A network change waits for confirmation`; `Nothing to confirm`; `Confirm at the device as well` |
| `-222,Data out of range` | frequency outside 8 kHz–160 MHz (also as `frequencyText`), correction beyond ±5000, backlight outside 10–100, more than two `networks` or `delete` entries, an integer beyond 64 bits |
| `-224,Illegal parameter value` | a value outside its list (master, phase, drive, step, `tuning`, `spread`, `offState`, `mode`, `slot` other than 0/1), `frequencyText` that is no frequency, SSID not 1–32 bytes, a network key that is neither `""`, 8–63 characters nor 64 hex digits, AP password not 8–63, bad device name or address text, `"\u0000"` in a string |

Backlight takes every integer from 10 to 100 and rounds it to 10 % (half up: 55 → 60); it is never refused for not being a multiple of 10.

HTTP status codes (guard: `net/request_guard.cpp`, raw-byte pre-filter: `net/web_limits.cpp`, server: `net/web_server.cpp`):

| Status | `error` | Cause |
|---|---|---|
| 200 | | write done: body is the state; `GET` answers |
| 202 | | `POST /api/wifi`: `{"confirmWithin":120}` (0 for `off`) |
| 204 | | login done (with `Set-Cookie`); login without a web password set (nothing to log in to, no cookie); logout; `wifi/confirm` |
| 302 | | captive portal: page `GET`/`HEAD` with a foreign `Host` over the AP while captive DNS runs (`Location: http://192.168.4.1/`) |
| 304 | | asset with a matching `If-None-Match` |
| 400 | `-102` … `-224` | the table above |
| 400 | `-200,Bad request line` / `Bad Content-Length` / `Bad Cookie header` / `Bad Authorization header` / `Bad upgrade` / `WebSocket upgrade expected` | malformed request (guard) |
| 401 | `-200,Login required` | web password set and no valid session: every `POST` except login/logout, `GET /api/wifi*` |
| 401 | `-200,Wrong password` | login with a wrong password (counts for the lock) |
| 403 | `-200,Invalid token: reload the page` | `X-CYD-Token` (or `t=` of the WebSocket) missing or stale after a restart |
| 403 | `-200,Foreign origin` / `Cross-site request` / `Same-origin only` | `Origin` not the device, `Sec-Fetch-Site: cross-site`, `GET /api/token` not same-origin |
| 403 | `-221,Remote outputs: local only` | switching on or `offState` `high` under `local only` (D22) |
| 403 | `-221,Remote outputs can only be changed at the device` | `remoteOutputs` in `POST /api/config` |
| 403 | `-221,Passwords cannot be sent over the open access point` | any `POST` body with a password field (login, WiFi) over an open AP (W-S1) |
| 404 | `-200,Not found` / `-114,Header suffix out of range` | unknown path; `/api/channel/<n>` other than 1 and 2 |
| 405 | `-200,OPTIONS not allowed` / `Method not allowed` | `OPTIONS`, a method other than `GET`/`HEAD`/`POST`, `POST` outside `/api/` |
| 411 | `-200,Length required` | `POST` without `Content-Length` |
| 413 | `-200,Body too large` / `-223,Too much data` | body > 1 KB (by `Content-Length`, the raw bytes or the parser) |
| 414 | `-200,URL too long` | path and query > 256 bytes |
| 415 | `-200,Content-Type must be application/json` / `Transfer-Encoding not allowed` | `POST` not JSON; any chunked request |
| 421 | `-200,Host not allowed` | `Host` not the own address, `<name>` or `<name>.local` (no IPv6 literals) |
| 429 | `-200,Too many commands` | more than 20 writes/s from the web |
| 429 | `-200,Scan at most every 10 s` | scan started again within 10 s |
| 429 | `-200,Too many failed logins` + `retryAfter` | login lock (5 failures → 30 s, doubling up to 16 min) |
| 431 | `-200,Headers too large` | headers > 2 KB |
| 500 | `-240,SI5351 setup failed` | the write was applied but the SI5351 refused it (hardware error) |
| 500 | `-200,Session failed` | login correct, but no session could be started |
| 503 | `-200,Low memory` / `Out of memory` | free heap < 24 KB at the guard, or no memory for the answer or the JSON document |
| 503 | `-200,Busy, try again` | command queue full, the loop did not answer in time, login check busy |
| 503 | `-200,Too many connections` | a 5th HTTP connection (at most 4 at a time; raw answer before parsing) |
| 503 | `-200,Too many clients (max. 3)` | page or a 4th WebSocket while 3 are open |

Not reproduced by `tools/web_mock.py`: 302 (no captive DNS), 304 (no ETag), 411, 431, 500 (no hardware), the `Too many connections`, `Low memory`/`Out of memory` and `Busy, try again` answers, `-223`, the malformed-request 400s of the guard and `-114` (the mock answers `/api/channel/3` with 404 `-200,Not found`); it answers 413 only by `Content-Length`.

`frequencyText` in the mock: digits with blanks between groups, one `.` or `,`, optional unit `Hz`, `k`/`kHz`, `M`/`MHz` (any case); a decimal needs `k` or `M`; rounded to whole Hz; no sign, exponent or hex.

Scan: the first GET starts a scan (`running`), the GET after it finished returns `done` once; another start within 10 s of the previous start answers 429. The UI polls once per second while `running`.

## Settled in S55 (firmware = reference, the mock follows it)

- `wifi.confirmWithin` (seconds left, 0 = nothing pending) is part of the state; a page opened at the new address shows `Confirm`.
- Mode `off` over the web is final: applied after the answer and saved, `confirmWithin` 0, the UI says WiFi switches off. The body is then exactly `{"mode":"off"}`: any other key (also an empty `networks`, `delete`, `staticIp` or `ap`) answers 400 `-108,Parameter not allowed` and changes nothing, because the other fields would be saved without the confirmation (D23, audit SEC-2). The UI refuses such a combination before sending: `WiFi off takes no other changes: apply them first`.
- `device.passwordSet` tells whether a web password is set; it is set at the device only (no REST call, WIFI > Device > `Web pw.`). Setting, changing or removing it ends every session; the UI then hides `Log out` until the next login. Without one, a network change needs `Keep` at the touch screen as well: a web `Confirm` before that answers `-221,Confirm at the device as well` and counts once the touch confirms.
- Stale token after a restart: 403 `-200,Invalid token: reload the page`.
- Error codes: see [Errors](#errors) (the earlier list here was merged into it, DOC-2).
- `/api/state` and `/ws` are readable without login also when a password is set (they hold no secrets, MAC or commit).
- Without connection `wifi.ssid`/`ip` are `""`, `rssi` 0; with the AP `ip` is the AP address.
- No REST call for `Defaults` / `Factory`, touch calibration, screen lock, AP `Regenerate` and the web password; the UI says these are set at the device.
- `PgUp`/`PgDn` are read as frequency × 10 / ÷ 10 (ARBEITSPLAN 3.6 "× 10 / ÷ 10"); `←`/`→` already step the step size by decades.

## Settled in S56

- `POST /api/logout` with `{}`: 204 and an expired cookie, also without a valid session (an expired one must not ask for the password first); other keys `-108`. The UI shows `Log out` once a request that needs the session succeeded (login, WiFi settings, a write) and hides it on 401, after the logout and when `device.passwordSet` changes; after the logout it clears the WiFi form (it came with the session).
- The UI uses `S.ch3.master` only as 1 or 2 (CodeQL, G4).
