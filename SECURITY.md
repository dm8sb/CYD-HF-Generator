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

- the network interfaces of the firmware: WiFi client and access point (including the fallback access point, captive DNS and mDNS), SCPI over TCP (port 5025), the web server with its REST API and WebSocket, and the static web UI in `data/www`,
- SCPI over USB serial,
- handling of credentials stored in NVS (WiFi passphrases, web password hash) and their exposure through any interface, log or export,
- input parsing reachable from these interfaces (SCPI parser, JSON, request limits, string sanitizing),
- the host tools in `tools/` and the GitHub workflows of this repository.

Out of scope:

- attacks that need physical access to the device: reading or rewriting the flash over USB/UART or JTAG, the touch screen, factory reset. Secure Boot and Flash Encryption are deliberately not used (ARBEITSPLAN S-22), so anyone with physical access has full access, including the stored WiFi passphrases,
- the lack of TLS/HTTPS: traffic on the local network is plain text by design; the user manual recommends a guest or IoT network,
- denial of service by flooding the WiFi channel or the network,
- vulnerabilities in third-party components (Arduino-ESP32/ESP-IDF, LVGL, TFT_eSPI, scpi-parser, ArduinoJson, ...) that are not specific to how this firmware uses them; please report those upstream. A note that this project pins an affected version is still welcome.

## What to expect

- Acknowledgement: usually within 7 days.
- Assessment: within about 30 days, with a plan for a fix or the reason why it is not considered a vulnerability.
- Fix: depends on severity and effort; critical network-reachable issues get priority. The advisory is published together with the fixed release, and reporters are credited unless they prefer otherwise.

There is no bug bounty.
