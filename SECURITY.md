# Security policy

CYD HF Generator is a hobby project maintained by one person in spare time. Reports are welcome and taken seriously; please be patient with response times.

## Supported versions

| Version | Supported |
|---|---|
| Latest release (see [Releases](https://github.com/dm8sb/CYD-HF-Generator/releases) and tags `v*`) | yes |
| Current development branch (`dev`, feature branches merged into it) | yes, fixes land here first |
| Older releases | no, please update to the latest release |

Fixes are released as a new version; there are no backports to older releases.

## Reporting a vulnerability

Please report vulnerabilities **privately** through GitHub: open the repository's **Security** tab and choose **Report a vulnerability** ([direct link](https://github.com/dm8sb/CYD-HF-Generator/security/advisories/new), private vulnerability reporting). Do **not** open a public issue, pull request or discussion for a vulnerability.

A useful report contains:

- firmware version and commit (CONFIG page, info card) and the board (CYD or CYD2USB),
- the affected interface and its settings (WiFi client or access point, open or WPA2 access point, web password set or not, `Remote outputs`),
- steps to reproduce, ideally a minimal request or SCPI line sequence,
- the impact you observed or expect.

Please do not test against devices you do not own.

## Scope

In scope:

- the network interfaces of the firmware: WiFi client and access point (including the fallback access point, captive DNS and mDNS), SCPI over TCP (port 5025), VXI-11 with its portmapper (UDP/TCP 111, TCP 9009) and the LXI identification (`/lxi/identification`), the web server with its REST API and WebSocket, and the static web UI in `data/www`,
- SCPI over USB serial,
- handling of credentials stored in NVS (WiFi passphrases, web password hash) and their exposure through any interface, log or export,
- input parsing reachable from these interfaces (SCPI parser, JSON, request limits, string sanitizing),
- the host tools in `tools/` and the GitHub workflows of this repository.

Out of scope:

- attacks that need physical access to the device: reading or rewriting the flash over USB/UART or JTAG, the touch screen, factory reset. Secure Boot and Flash Encryption are deliberately not used (ARBEITSPLAN S-22), so anyone with physical access has full access, including the stored WiFi passphrases,
- the lack of TLS/HTTPS: traffic on the local network is plain text by design; the user manual recommends a guest or IoT network,
- the lack of authentication on network SCPI (decision E-41 in `PROJECT_SPECIFICATION.md`): SCPI over TCP 5025 and VXI-11 accept every command (frequencies, settings, `*RST`, `SYST:SAVE`, outputs) from anyone who reaches the device, by design and as usual for lab instruments; the web password protects only the web interface and REST, and there is no setting to switch network SCPI off. Both are blocked over an open access point, and `Remote outputs: local only` keeps the outputs from being switched on and the off state from being set to High remotely (user manual sections 14 and 15). That a network client can hold the single SCPI connection (each line restarts the idle timer; `Reconnect all` at the device frees it) is part of this. A way around these limits (for example network SCPI over an open access point, or switching an output on under `local only`) is in scope. `/lxi/identification` is readable without login on purpose; it holds only what `*IDN?` returns,
- denial of service by flooding the WiFi channel or the network,
- vulnerabilities in third-party components (Arduino-ESP32/ESP-IDF, LVGL, TFT_eSPI, scpi-parser, ArduinoJson, ...) that are not specific to how this firmware uses them; please report those upstream. A note that this project pins an affected version is still welcome.

## Release integrity

Releases are built by `.github/workflows/release.yml` from a `v*` tag whose commit is on `main` (merged through a pull request with green checks) and passes the host tests. Tags `v*` cannot be moved or deleted (tag ruleset) and published releases cannot be changed (immutable releases); the workflow also refuses to touch an existing release. Every asset is listed in `SHA256SUMS` and covered by a build provenance attestation (`gh attestation verify <file> --repo dm8sb/CYD-HF-Generator`). The web flasher on GitHub Pages is deployed only by a tag, after its release. A release asset that does not verify, or a flasher that serves something else than the release, is in scope.

## What to expect

- Acknowledgement: usually within 7 days.
- Assessment: within about 30 days, with a plan for a fix or the reason why it is not considered a vulnerability.
- Fix: depends on severity and effort; critical network-reachable issues get priority. The advisory is published together with the fixed release, and reporters are credited unless they prefer otherwise.

There is no bug bounty.
