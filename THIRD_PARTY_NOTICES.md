# Third-Party Notices

CYD HF Generator, Copyright (C) 2026 DM8SB

SPDX-License-Identifier: GPL-3.0-or-later

CYD HF Generator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version (`LICENSE`), with an additional permission under GPLv3 section 7 for linking with the libraries Espressif distributes only in object code form (`LICENSE-EXCEPTION.md`). The firmware image built from this repository also contains third-party software under its own licenses, and the web flasher serves ESP Web Tools with its bundled components. This file lists every component, what it is used for, and where its license text is.

Checked on 2026-11-12 (`espressif32@8.0.0`): versions are the ones pinned in `platformio.ini` and `flasher/esp-web-tools.json`, checked against the downloaded packages, the archive members of the linker map of `pio run -e esp32-2432S028R` (`firmware.map`) and Espressif's list of firmware components (see "Compared with Espressif's list" below). **Update this file with every dependency change in `platformio.ini` or `flasher/esp-web-tools.json`** (gate, CLAUDE.md), and repeat the comparison with the linker map and Espressif's list when the platform changes.

## Where the license texts are

| Location | Content |
|---|---|
| `LICENSE` | GPL-3.0 text (this project, GPL-3.0-or-later; also the "GNU GPL" that LGPL-3.0 refers to) |
| `LICENSE-EXCEPTION.md` | Additional permission under GPLv3 section 7 for Espressif's object-code-only libraries (E-38) |
| `LICENSES/` | Full texts of the other licenses named below (SPDX texts; the MIT/BSD texts are templates, the copyright lines are in this file) and `TFT_eSPI-license.txt`, the complete license file of TFT_eSPI 2.5.43 as shipped (the release build checks that it still matches the pinned package) |
| Release asset `…-licenses.tar.gz`, flasher site `notices/` | This file, `LICENSE`, `LICENSE-EXCEPTION.md` (as `LICENSE-EXCEPTION.txt`), `LICENSES/`, and the license files exactly as shipped in the pinned packages (`upstream/`) |
| Release asset `…-lgpl-sources.tar.gz`, flasher site `notices/lgpl-sources.tar.gz` | Complete source of the LGPL parts linked into the firmware: Arduino-ESP32 core, libraries, board variant `variants/esp32`, partition tables and the build scripts `tools/platformio-build.py` and `tools/pioarduino-build.py`; AsyncTCP; ESPAsyncWebServer; the LGPL 2.1, LGPL 3.0 and GPL 3.0 texts; a `README.txt` naming the Arduino-ESP32 upstream tag, the PlatformIO package versions and the ESP-IDF commit of the precompiled libraries |
| Flasher site `notices/esp-web-tools/` | The license files of ESP Web Tools and its bundled packages as published on npm, and the source archives of the flasher stubs (GPL-2.0-or-later) the site serves; pinned with SHA-256 in `flasher/esp-web-tools.json` |

The complete source of this project is the tagged commit (GitHub adds `Source code (zip/tar.gz)` to every release). The libraries are pinned exactly in `platformio.ini`; the Arduino-ESP32 framework packages come with the pinned platform `espressif32@8.0.0` (their versions are in the table below and in the `README.txt` of the LGPL archive; an exact pin of the framework packages themselves is ARBEITSPLAN AF22). `pio run -e esp32-2432S028R` on that commit rebuilds the firmware, including against a modified library (LGPL relinking).

## Overview

Everything in this repository that is not listed below as a third-party component (firmware sources, web UI, flasher page, tools, tests, documentation and the case in `case/`; not the license texts in `LICENSES/`) is under the repository license, GPL-3.0-or-later. The source files say so in two SPDX lines (`SPDX-FileCopyrightText: 2026 DM8SB`, `SPDX-License-Identifier: GPL-3.0-or-later`); `REUSE.toml` states the same for files without comments (documentation, JSON, fuzz dictionaries and seeds, images).

Credit: the CYD dimensions of the case in `case/` were measured from the STEP model "Cheap Yellow Display" by mbrembati; only measurements were taken, no file of that model is in this repository.

"Firmware" = linked into `firmware.bin` (and, where noted, the bootloader). "Flasher" = loaded by the visitor's browser on the GitHub Pages flasher. "Tests" and "Tools" = used only on the developer's or CI machine, not distributed.

| Component | Version | License (SPDX) | Copyright | Source | Used in |
|---|---|---|---|---|---|
| CYD HF Generator (this project, including the web UI in `data/www`, the flasher page and the OpenSCAD case in `case/`) | see tag | GPL-3.0-or-later | DM8SB | https://github.com/dm8sb/CYD-HF-Generator | Firmware, Flasher, Case |
| TFT_eSPI | 2.5.43 | BSD-2-Clause-Views AND BSD-2-Clause AND MIT | Bodmer; Adafruit Industries; Limor Fried | https://github.com/Bodmer/TFT_eSPI | Firmware (display driver only; its fonts are not built since 2026-11-10) |
| LVGL | 8.4.0 | MIT | LVGL Kft | https://github.com/lvgl/lvgl | Firmware |
| ↳ QR Code generator (lv_qrcode) | in LVGL 8.4.0 | MIT | Project Nayuki | https://www.nayuki.io/page/qr-code-generator-library | Firmware |
| ↳ TLSF allocator (lv_tlsf) | in LVGL 8.4.0 | BSD-3-Clause | Matthew Conte | http://tlsf.baisoku.org | Firmware |
| ↳ printf (lv_printf) | in LVGL 8.4.0 | MIT | Marco Paland, PALANDesign Hannover | https://github.com/mpaland/printf | Firmware |
| ↳ Montserrat (bitmap fonts 12, 14, 16, 20, 28 px) | in LVGL 8.4.0 | OFL-1.1 | The Montserrat Project Authors | https://github.com/JulietaUla/Montserrat | Firmware |
| ↳ Font Awesome 5 Free (symbol glyphs in the same fonts) | in LVGL 8.4.0 | OFL-1.1 (font files) | Fonticons, Inc. | https://fontawesome.com | Firmware |
| XPT2046_Touchscreen | 1.4, commit `d57f64c` | MIT | Paul Stoffregen | https://github.com/PaulStoffregen/XPT2046_Touchscreen | Firmware |
| scpi-parser | commit `8866159` (v2.3 + fixes) | BSD-2-Clause | Jan Breuer | https://github.com/j123b567/scpi-parser | Firmware, Tests |
| ArduinoJson | 7.4.3 | MIT | Benoit Blanchon | https://github.com/bblanchon/ArduinoJson | Firmware, Tests |
| AsyncTCP | 3.5.0 | LGPL-3.0-or-later | Hristo Gochkov, Mathieu Carbou, Emil Muratov, Will Miles | https://github.com/ESP32Async/AsyncTCP | Firmware |
| ESPAsyncWebServer | 3.12.1 | LGPL-3.0-or-later | Hristo Gochkov, Mathieu Carbou, Emil Muratov, Will Miles | https://github.com/ESP32Async/ESPAsyncWebServer | Firmware |
| Arduino-ESP32 core and libraries (WiFi, Network, ESPmDNS, DNSServer, AsyncUDP, Wire, SPI, Preferences, FS, Hash) | 3.3.12 | LGPL-2.1-or-later AND Apache-2.0 AND MIT, libb64 public domain (per file, note 5) | Arduino Team; Ivan Grokhotkov; Hristo Gochkov; Espressif Systems (Shanghai) Co., Ltd. and contributors | https://github.com/espressif/arduino-esp32 | Firmware |
| ↳ ESPmDNS | in Arduino-ESP32 3.3.12 | MIT | Tony DiCola; Ivan Grokhotkov; Hristo Gochkov; Lars Englund | https://github.com/espressif/arduino-esp32/tree/3.3.12/libraries/ESPmDNS | Firmware |
| ↳ libb64 (`cores/esp32/libb64`, Base64 encoder) | in Arduino-ESP32 3.3.12 | public domain (copyright-only dedication) | Chris Venter | https://github.com/espressif/arduino-esp32/tree/3.3.12/cores/esp32/libb64 | Firmware |
| ESP-IDF (system, drivers, NVS, netif, WiFi/BT glue, bootloader) | v5.5.5 (`b774170ff46`) | Apache-2.0 | Espressif Systems (Shanghai) Co., Ltd. | https://github.com/espressif/esp-idf | Firmware, bootloader |
| ↳ WiFi, PHY, BT controller binary libraries (net80211, pp, mesh, smartconfig, coexist, espnow, phy, rtc, btdm_app) | with ESP-IDF v5.5.5 | Apache-2.0 | Espressif Systems | https://github.com/espressif/esp32-wifi-lib, esp-phy-lib, esp32-bt-lib | Firmware |
| ↳ FreeBSD net80211 (code inside the binary library `libnet80211`) | with ESP-IDF v5.5.5 | BSD-2-Clause (Espressif: "BSD License") | Sam Leffler, Errno Consulting | https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/COPYRIGHT.html | Firmware |
| ↳ FreeRTOS kernel | V10.5.1 (ESP-IDF port) | MIT (Espressif changes Apache-2.0) | Amazon.com, Inc.; Espressif Systems | https://github.com/espressif/esp-idf/tree/v5.5.5/components/freertos | Firmware |
| ↳ lwIP | 2.2.0 (ESP-IDF fork) | BSD-3-Clause | Swedish Institute of Computer Science and contributors | https://github.com/espressif/esp-lwip | Firmware |
| ↳ Mbed TLS | 3.6.6 (ESP-IDF fork) | Apache-2.0 (dual Apache-2.0 OR GPL-2.0-or-later, used under Apache-2.0) | The Mbed TLS Contributors | https://github.com/espressif/mbedtls | Firmware |
| ↳ wpa_supplicant / hostapd (incl. Fast PBKDF2, CC0-1.0) | ESP-IDF v5.5.5 | BSD-3-Clause | Jouni Malinen and contributors | https://w1.fi | Firmware |
| ↳ TLSF heap allocator | ESP-IDF v5.5.5 | BSD-3-Clause | Matthew Conte | https://github.com/espressif/tlsf | Firmware |
| ↳ cJSON | 1.7.19 | MIT | Dave Gamble and cJSON contributors | https://github.com/DaveGamble/cJSON | Firmware |
| ↳ protobuf-c | ESP-IDF v5.5.5 | BSD-2-Clause | Dave Benson and the protobuf-c authors | https://github.com/protobuf-c/protobuf-c | Firmware |
| ↳ Xtensa HAL | ESP-IDF v5.5.5 | MIT | Cadence Design Systems, Inc. | https://github.com/espressif/esp-idf/tree/v5.5.5/components/xtensa | Firmware |
| ↳ espressif/mdns | 1.13.1 | Apache-2.0 | Espressif Systems | https://components.espressif.com/components/espressif/mdns | Firmware |
| ↳ espressif/network_provisioning | 1.0.2 | Apache-2.0 | Espressif Systems | https://components.espressif.com/components/espressif/network_provisioning | Firmware |
| ↳ espressif/esp_diagnostics | 1.3.4 | Apache-2.0 | Espressif Systems | https://components.espressif.com/components/espressif/esp_diagnostics | Firmware |
| newlib (libc, libm) | esp-14.2.0 toolchain | BSD-style, per file (see `COPYING.NEWLIB`) | Red Hat, Inc.; The Regents of the University of California; others | https://github.com/espressif/newlib-esp32 | Firmware, bootloader |
| GCC runtime (libgcc, libstdc++) | 14.2.0 (esp-14.2.0_20260121) | GPL-3.0-or-later WITH GCC-exception-3.1 | Free Software Foundation, Inc. | https://github.com/espressif/gcc | Firmware (no obligations under the runtime exception) |
| `boot_app0.bin`, partition table `huge_app.csv` | Arduino-ESP32 3.3.12 | data files of LGPL-2.1-or-later package | Espressif Systems | https://github.com/espressif/arduino-esp32/tree/3.3.12/tools/partitions | Release/flasher parts |
| ESP Web Tools (`dist/web`, 26 modules) | 10.4.0 | Apache-2.0 | ESPHome maintainers (Open Home Foundation) | https://github.com/esphome/esp-web-tools | Flasher (served by the flasher site, E-36) |
| ↳ esptool-js | 0.6.0 | Apache-2.0 | Espressif Systems (Shanghai) Co., Ltd. | https://github.com/espressif/esptool-js | Flasher |
| ↳ flasher stubs (RAM programs sent to the chip while flashing, in `stub_flasher_*.js`) | esptool-legacy-flasher-stub v1.3.0 (ESP32 and most chips), v1.6.0 (ESP32-P4 rc1), v1.10.0 (ESP32-P4), v1.11.1 (ESP32-C5), see note 4 | GPL-2.0-or-later | Cesanta Software Limited; Espressif Systems (Shanghai) Co., Ltd. | https://github.com/espressif/esptool-legacy-flasher-stub | Flasher (source on the site) |
| ↳ pako | 2.2.0 | MIT AND Zlib | Vitaly Puzrin, Andrei Tuputcyn; Jean-loup Gailly, Mark Adler (zlib) | https://github.com/nodeca/pako | Flasher |
| ↳ atob-lite | 2.0.0 | MIT | Hugh Kennedy | https://github.com/hughsk/atob-lite | Flasher |
| ↳ improv-wifi-serial-sdk | 2.8.0 | Apache-2.0 | Improv Wi-Fi maintainers | https://github.com/improv-wifi/sdk-serial-js | Flasher |
| ↳ @material/web | 2.4.1 | Apache-2.0 | Google, Inc. | https://github.com/material-components/material-web | Flasher |
| ↳ lit, lit-html, lit-element, @lit/reactive-element | 3.3.3, 3.3.0, 4.2.0, 2.1.0 | BSD-3-Clause | Google LLC | https://github.com/lit/lit | Flasher |
| ↳ tslib | 2.8.1 | 0BSD | Microsoft Corporation | https://github.com/microsoft/tslib | Flasher |
| Unity | 2.6.1 | MIT | Mike Karlesky, Mark VanderVoord, Greg Williams | https://github.com/ThrowTheSwitch/Unity | Tests |
| PlatformIO Core, platform espressif32 | 6.2.0, 8.0.0 | Apache-2.0 | PlatformIO Labs | https://github.com/platformio | Tools |
| Xtensa GCC toolchain, binutils | 14.2.0 | GPL-3.0-or-later and others | Free Software Foundation and others | https://github.com/espressif/crosstool-NG | Tools |
| esptool | 5.x (via PlatformIO) | GPL-2.0-or-later | Espressif Systems, Fredrik Ahlberg, Angus Gratton | https://github.com/espressif/esptool | Tools |
| pyserial, PyVISA, PyVISA-py (optional, installed by the user for `tools/`) | any | BSD-3-Clause; MIT; MIT | Chris Liechti; PyVISA authors | https://pypi.org | Tools |
| GitHub Actions (`actions/*`, `github/codeql-action`) | pinned by SHA in `.github/workflows` | MIT | GitHub, Inc. | https://github.com/actions | CI |
| CodeQL CLI and query packs (downloaded by `github/codeql-action` in `codeql.yml`) | bundle of the pinned action | GitHub CodeQL Terms and Conditions (proprietary; free for open source and research; queries MIT) | GitHub, Inc. | https://github.com/github/codeql-cli-binaries/blob/main/LICENSE.md | CI (runs on GitHub's runners, not distributed) |
| clang / libFuzzer (`test/fuzz`) | system | Apache-2.0 WITH LLVM-exception | LLVM project | https://llvm.org | Tests |
| Python 3 (`tools/`, PlatformIO) | 3.x | PSF-2.0 | Python Software Foundation | https://www.python.org | Tools |
| OpenSCAD (`case/`, `tools/render_images.py case`) | development snapshot (`--backend=Manifold`) | GPL-2.0-or-later | Marius Kintel, Clifford Wolf and contributors | https://openscad.org | Tools (developer machine, not distributed; the `.scad` files are this project's own) |
| Google Chrome or Microsoft Edge (headless, `tools/render_images.py`, `tools/cdp_shot.mjs`) | any current | proprietary (Chromium parts BSD-3-Clause and others) | Google LLC; Microsoft Corporation | https://www.google.com/chrome, https://www.microsoft.com/edge | Tools (developer machine, not distributed) |
| Node.js (`tools/cdp_shot.mjs`) | 22+ | MIT (bundled parts under their own licenses) | OpenJS Foundation and Node.js contributors | https://nodejs.org | Tools (developer machine, not distributed) |

Notes:

1. **Only parts in the linker map are listed for ESP-IDF.** The prebuilt Arduino-ESP32 libraries contain many more ESP-IDF components (Matter, Zigbee, RainMaker, camera, SPIFFS, FatFS, Bluedroid, …); they are not linked into this firmware. Espressif's complete list: https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32/COPYRIGHT.html. The Arduino `SPIFFS` library and the ESP-IDF `spiffs` component (MIT, Peter Andersson) are not linked either (no member of `libSPIFFS.a` or `libspiffs.a` in the map).
2. **ESPAsyncWebServer `BackPort_SHA1Builder.*`** (Apache-2.0, ARM Limited / Espressif) is compiled only for ESP-IDF < 5 and is not in this firmware.
3. **TFT_eSPI GFX free fonts** (`Fonts/GFXFF`, GNU FreeFont) are not built (`LOAD_GFXFF` is not set). `src/drivers/display_detect.cpp` contains the ST7789 command sequence of TFT_eSPI 2.5.43 `TFT_Drivers/ST7789_Init.h` (FreeBSD license = BSD-2-Clause-Views, Bodmer), see below.
4. **ESP Web Tools** is served by the flasher site itself (E-36): the release workflow downloads the 26 pinned modules of `dist/web` (SHA-384 in `flasher/esp-web-tools.json`), checks them and publishes them under `esp-web-tools/10.4.0/`; the page makes no request to another origin. The bundle has its license comments stripped (`terser`, `comments: false`), so the bundled packages above were taken from the `package-lock.json` of the ESP Web Tools tag 10.4.0 and confirmed by the version strings in the bundle (lit 3.3.0 / 4.2.0 / 2.1.0); their license files are on the site (`notices/esp-web-tools/`). `@lit-labs/ssr-dom-shim` and `@types/trusted-types` are dependencies for Node.js and TypeScript only and are not in the browser bundle. The **flasher stubs** in `stub_flasher_*.js` were identified by comparing their code and data segments with the released stub images: ESP32, ESP32-S2/S3, C2, C3, C6, C61, H2 and ESP8266 are identical to esptool-legacy-flasher-stub v1.3.0 (shipped with esptool 4.8.1), ESP32-P4 rc1 to v1.6.0, ESP32-P4 to v1.10.0 and ESP32-C5 to v1.11.1. They are GPL-2.0-or-later (the README in esptool-js says so, too); the complete source of these four releases is published next to them (`notices/esp-web-tools/esptool-legacy-flasher-stub-v*.tar.gz`, GPL-2.0 section 3: equivalent access from the same place). The ESP8266 stub also contains miniz 2.1.0 (MIT, Rich Geldreich, RAD Game Tools and Valve Software). Only the ESP32 stub is ever loaded by this flasher.
5. **Arduino-ESP32** mixes files under several licenses; each file header is authoritative, and files without a header fall under the repository license (LGPL-2.1-or-later). Of the linked files: LGPL-2.1-or-later (core API such as `Print`, `Stream`, `WString`, `IPAddress`, `Esp`; `WiFi*`, `NetworkClient`/`NetworkServer`, `FS`, `Wire`, `SPI`; no header: `HardwareSerial`, `main`, `AsyncUDP`, `DNSServer`), Apache-2.0 (`esp32-hal-*`, `Network{Events,Interface,Manager}`, `WiFi/AP`, `WiFi/STA`, `Preferences`, `SHA1Builder`), MIT (`ESPmDNS`; the headers `Udp.h` and `NetworkUdp.h`) and public domain (libb64 `cencode.c`, used for Base64).
6. **Object-code-only libraries** (`LICENSE-EXCEPTION.md`, E-38), bytes in the image from `firmware.map` (flash / IRAM+DRAM): `libnet80211` 169 630 / 9 430, `libpp` 66 670 / 6 118, `libphy` 34 226 / 12 621, `libcoexist` 6 473 / 3 738, `libsmartconfig` 1 280 / 208, `libcore` 311 / 9, `librtc` 0 / 1 754, `libespnow` 0 / 3, `libmesh` 0 / 0, `libbtdm_app` 21 / 447. The Bluetooth controller library is pulled in although the firmware uses no Bluetooth: Arduino's `initArduino()` releases the BT memory (`btMemRelease()` → `esp_bt_mem_release()` in ESP-IDF `bt.c`), which references 71 members of `libbtdm_app`; after `--gc-sections` only about 0.5 KB of them remain. The Bluetooth host (Bluedroid) is not linked.

### Compared with Espressif's list (G11 gate)

Espressif's `COPYRIGHT` page for ESP-IDF v5.5.5 lists the third-party "Firmware Components" of ESP-IDF. Each entry was checked against the archive members in `firmware.map` of `pio run -e esp32-2432S028R` (2026-11-12) and the symbols of the prebuilt bootloader (`bootloader_*.elf` of `framework-arduinoespressif32-esp32-libs` 3.3.12):

| Espressif entry | In this firmware | Evidence |
|---|---|---|
| Newlib | yes (table above) | `libc.a`, `libm.a` of the toolchain |
| Picolibc | no | newlib is the C library |
| Xtensa header files | yes ("Xtensa HAL" above) | `libhal.a`, `libxt_hal.a`, `libxtensa.a` |
| FreeRTOS | yes | `libfreertos.a` |
| LWIP | yes | `liblwip.a` |
| wpa_supplicant, Fast PBKDF2 | yes | `libwpa_supplicant.a`, member `fastpbkdf2.c.obj` |
| FreeBSD net80211 | yes (inside the binary library) | `libnet80211.a`, 35 members |
| cJSON | yes | `libjson.a(cJSON.c.obj)`, pulled in by `espressif/network_provisioning` |
| Mbed TLS | yes | `libmbedcrypto.a`, `libmbedtls*.a`, `libmbedx509.a` |
| TLSF allocator | yes | `libheap.a(tlsf.c.obj)` |
| protobuf-c | member linked, no code left after `--gc-sections` | `libprotobuf-c.a(protobuf-c.c.obj)`, pulled in by `espressif/network_provisioning` |
| argtable3, linenoise, FatFS, micro-ecc, SPIFFS, SD/MMC driver, ESP-MQTT, BLE Mesh, mynewt-nimble, openthread, UBSAN runtime, HTTP Parser, SEGGER SystemView, CMock, Unity (ESP-IDF copy) | no | no member of their libraries in the map; the bootloader has no `uECC` symbol (secure boot is off in the prebuilt `sdkconfig`) |

The ROM code listed there (newlib, libhal, TinyBasic Plus, miniz, TJpgDec, Zephyr USB parts) is in the chip's mask ROM, not in the image.

## Notices

The MIT, BSD, zlib and OFL licenses require the following notices to accompany binary distributions. The license texts themselves are in `LICENSES/` (`MIT.txt`, `BSD-2-Clause.txt`, `BSD-3-Clause.txt`, `0BSD.txt`, `Zlib.txt`, `OFL-1.1.txt`, `Apache-2.0.txt`, `GPL-2.0-or-later.txt`, `LGPL-2.1-or-later.txt`, `LGPL-3.0-or-later.txt`, `GCC-exception-3.1.txt`, `TFT_eSPI-license.txt`) and, as shipped upstream, in the `upstream/` folder of the release notices and in `notices/esp-web-tools/` of the flasher site.

### LGPL components (Arduino-ESP32, AsyncTCP, ESPAsyncWebServer)

This firmware uses the Arduino-ESP32 core and libraries (GNU Lesser General Public License 2.1 or later) and AsyncTCP and ESPAsyncWebServer (GNU Lesser General Public License 3.0 or later). These libraries and their use are covered by those licenses; copies of the LGPL 2.1, the LGPL 3.0 and the GNU GPL 3.0 are in `LICENSES/` and `LICENSE`. The complete corresponding source of these libraries is published with every release (`…-lgpl-sources.tar.gz`), and the complete source of the firmware that uses them is the tagged commit of this repository, so the firmware can be rebuilt and relinked against modified versions of the libraries with `pio run`.

### TFT_eSPI

```
Copyright (c) 2023 Bodmer (https://github.com/Bodmer)
All rights reserved.
Software License Agreement (FreeBSD License) — full text in LICENSES/TFT_eSPI-license.txt
plus: "The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project."

Adafruit_GFX functions:   Copyright (c) 2012 Adafruit Industries. All rights reserved. (BSD License)
Adafruit_ILI9341 origin:  Written by Limor Fried/Ladyada for Adafruit Industries.
                          MIT license, all text above must be included in any redistribution
```

The complete TFT_eSPI `license.txt` is in the repository (`LICENSES/TFT_eSPI-license.txt`) and in the release notices (`upstream/TFT_eSPI-license.txt`). The ST7789 init sequence in `src/drivers/display_detect.cpp` is taken from TFT_eSPI 2.5.43 `TFT_Drivers/ST7789_Init.h` under the same license.

### LVGL and its bundled parts

```
LVGL: MIT licence, Copyright (c) 2021 LVGL Kft
QR Code generator library (C): Copyright (c) Project Nayuki. (MIT License)
Two Level Segregated Fit memory allocator, version 3.1: Copyright (c) 2006-2016, Matthew Conte. All rights reserved. (BSD-3-Clause)
printf: (c) Marco Paland (info@paland.com), 2014-2019, PALANDesign Hannover, Germany (MIT License)
Montserrat: Copyright 2011 The Montserrat Project Authors (https://github.com/JulietaUla/Montserrat), SIL Open Font License 1.1
Font Awesome Free 5 by @fontawesome - https://fontawesome.com (font files: SIL Open Font License 1.1)
```

The Montserrat and Font Awesome glyphs are compiled into the firmware as bitmap fonts (LVGL `lv_font_conv`); they are not offered as font files. The OFL 1.1 text is in `LICENSES/OFL-1.1.txt`.

### XPT2046_Touchscreen

```
Touchscreen library for XPT2046 Touch Controller Chip
Copyright (c) 2015, Paul Stoffregen, paul@pjrc.com
(MIT License; the package ships no separate license file, the notice is in XPT2046_Touchscreen.h/.cpp)
```

### scpi-parser

```
BSD 2-Clause License
Copyright (c) 2012-2018, Jan Breuer
All rights reserved.
```

### ArduinoJson

```
The MIT License (MIT)
Copyright © 2014-2026, Benoit BLANCHON
```

### Arduino-ESP32: ESPmDNS and libb64

```nESP8266 Multicast DNS (port of CC3000 Multicast DNS library), Version 1.1
Copyright (c) 2013 Tony DiCola (tony@tonydicola.com)
ESP8266 port (c) 2015 Ivan Grokhotkov (ivan@esp8266.com)
MDNS-SD Support 2015 Hristo Gochkov (hristo@espressif.com)
Extended MDNS-SD support 2016 Lars Englund (lars.englund@gmail.com)
License (MIT license)

libb64: Base64 Encoding/Decoding Routines, Chris Venter
This is part of the libb64 project, and has been placed in the public domain.
```n
The MIT text is in `LICENSES/MIT.txt`; the full header and the libb64 dedication are in the release notices (`upstream/ESPmDNS-header.txt`, `upstream/libb64-LICENSE.txt`) and in the LGPL source archive.

### Espressif components (ESP-IDF, binary libraries, managed components, Arduino-ESP32 Apache-2.0 files)

```
Copyright 2015-2026 Espressif Systems (Shanghai) CO LTD
Licensed under the Apache License, Version 2.0 (LICENSES/Apache-2.0.txt).
```

ESP-IDF contains no `NOTICE` file for this version; Apache-2.0 section 4(d) therefore adds no further text. Mbed TLS is used under Apache-2.0.

### Third-party code inside ESP-IDF

```
FreeRTOS Kernel V10.5.1: Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved. (MIT)
lwIP: Copyright (c) 2001-2004 Swedish Institute of Computer Science. All rights reserved. (BSD-3-Clause)
wpa_supplicant/hostapd: Copyright (c) 2002-2022, Jouni Malinen <j@w1.fi> and contributors (BSD-3-Clause; years per file)
FreeBSD net80211 (in libnet80211): Copyright (c) 2004-2008 Sam Leffler, Errno Consulting. All rights reserved. (BSD-2-Clause)
Fast PBKDF2: Joseph Birr-Pixton, CC0 1.0 Public Domain Dedication
TLSF: Copyright (c) 2006-2016, Matthew Conte. All rights reserved. (BSD-3-Clause)
cJSON: Copyright (c) 2009-2017 Dave Gamble and cJSON contributors (MIT)
protobuf-c: Copyright (c) 2008-2022, Dave Benson and the protobuf-c authors. All rights reserved. (BSD-2-Clause)
Xtensa HAL: Copyright (c) 1999-2015 Cadence Design Systems, Inc. (MIT)
```

### newlib

The C library is newlib; its files carry several BSD-style licenses (Red Hat, Inc.; The Regents of the University of California; and others), collected in `COPYING.NEWLIB` of the toolchain (`upstream/newlib-COPYING.NEWLIB` in the release notices).

### ESP Web Tools and its bundled components (flasher site only)

```
ESP Web Tools 10.4.0: ESPHome maintainers, Apache License 2.0
esptool-js 0.6.0: Copyright 2019 Espressif Systems (Shanghai) CO LTD, Apache License 2.0
improv-wifi-serial-sdk 2.8.0: Improv Wi-Fi maintainers, Apache License 2.0
@material/web 2.4.1: Copyright 2018-2023 Google, Inc., Apache License 2.0
lit 3.3.3, lit-html 3.3.0, lit-element 4.2.0, @lit/reactive-element 2.1.0:
    Copyright (c) 2017 Google LLC. All rights reserved. (BSD 3-Clause License)
pako 2.2.0: Copyright (C) 2014-2017 by Vitaly Puzrin and Andrei Tuputcyn (MIT License);
    zlib port: (C) 1995-2013 Jean-loup Gailly and Mark Adler (zlib License)
atob-lite 2.0.0: Hugh Kennedy (MIT License)
tslib 2.8.1: Copyright (c) Microsoft Corporation. (0BSD)
```

None of the Apache-2.0 packages ships a `NOTICE` file. The license files as published are in `notices/esp-web-tools/` of the flasher site.

### Flasher stubs (GPL-2.0-or-later, flasher site only)

```
esptool-legacy-flasher-stub v1.3.0, v1.6.0, v1.10.0, v1.11.1
SPDX-FileCopyrightText: 2016 Cesanta Software Limited
SPDX-FileContributor: 2016-2022 Espressif Systems (Shanghai) CO LTD
SPDX-License-Identifier: GPL-2.0-or-later
```

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version (`LICENSES/GPL-2.0-or-later.txt`). The complete source of each release is on the flasher site next to the stubs (`notices/esp-web-tools/esptool-legacy-flasher-stub-v*.tar.gz`) and at https://github.com/espressif/esptool-legacy-flasher-stub. The stubs run on the chip only during flashing; they are not part of the firmware.

## No warranty

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. The same applies to the third-party components under their respective licenses. CYD HF Generator is a private, non-commercial hobby project of DM8SB.
