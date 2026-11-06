# CYD HF Generator

PlatformIO + Arduino-ESP32 firmware for a 3-channel HF square-wave generator with an SI5351 on the ESP32-2432S028R "Cheap Yellow Display" (original CYD and CYD2USB).

## Documentation

- [USER_MANUAL_DE.md](USER_MANUAL_DE.md): user manual, reference for all behavior (German); [USER_MANUAL_EN.md](USER_MANUAL_EN.md): English translation
- [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md): requirements and decisions
- [ARBEITSPLAN.md](ARBEITSPLAN.md): open findings, WEB-HMI concept and task list (German)
- [MESSPLAN.md](MESSPLAN.md): hardware measurement plan by priority (German)
- [CLAUDE.md](CLAUDE.md): architecture and development rules

## Features

- CH1 and CH2: independent outputs 8 kHz–160 MHz with the actual output frequency on the channel pages
- CH3: IQ follower of CH1 or CH2 with 0/90/180/270° (4.730–100 MHz; 180/270° via output inversion)
- Tuning modes `Fractional` (no dropouts), `Low noise` (integer MultiSynth) and `Integer` (frequency on a raster); spread spectrum on CH1; switched-off outputs Low, High or High-Z; drive 2–8 mA; frequency correction in ppm
- SI5351 on CN1 (GPIO22/27) with SDA/SCL in either order, identity check, supervision and automatic reconnect
- One firmware for both boards: the display controller (ILI9341 / ST7789) is detected at every boot
- LVGL GUI: MAIN with a status bar and a mode line for settings that differ from the defaults; touch calibration at the first start
- NVS persistence with 5 s autosave; all outputs start off
- Serial log with version, branch, commit and build time; host tests and CI
- Remote control with SCPI over USB (manual section 15)

## Changes

### 1.4.0 (in development on `dev`)

- SCPI over USB: IEEE 488.2 common commands, outputs, frequencies, CH3 and all settings; exact number parsing with units and MIN/MAX/DEF, error queue, log read-out (`SYST:LOG?`) while a session keeps the serial port free of log lines, `SYST:IDEN` blinks backlight and LED
- Arduino-ESP32 3.3.12 (`espressif32@8.0.0`, env `esp32-2432S028R-core3`) and LVGL 8.4.0
- WiFi groundwork (CONFIG `WiFi` and `Remote out.`, WiFi state and `REMOTE <source>` in the status bar, WiFi and heap lines on the info card; setup pages follow; default Off): network settings in NVS `hfnet`, client with up to two networks, fallback access point after 60 s, AP off as soon as the client has an address, mDNS, captive DNS; the factory reset also erases the WiFi settings

### 1.3.3

- CH3 page sets the master frequency and switches a waiting master on; holding +/- starts slowly and speeds up after 3 s and 6 s, pressure dropouts up to 70 ms are bridged
- Frequency correction can be typed on the numpad (tap the value in CONFIG)
- Screen lock (CONFIG `Screen` → `Lock`, hold 2 s to unlock), reset to defaults keeping the calibration, factory reset
- CH1/CH2 range 8 kHz–160 MHz (was 1–100 MHz): kHz display below 1 MHz, 10-character numpad, hints on the channel page; above 100 MHz exact to 1.5–3 Hz (next to 112.5 MHz it was off by up to 5 Hz)

### 1.3.2

- Header shows the actual state; mode line on MAIN and the channel pages; `Off state` = `High` needs a confirmation
- MAIN status bar (save state, `CONFIG`) instead of the buttons CH1–CH3; CONFIG ordered by signal settings, `Step` row removed
- Larger dialog and edge buttons, better contrast, states as filled labels (`! ERR`, `! NO HW`), step chips in digit order
- Numpad keeps a rejected input and shows why; output error dialogs name the channel
- Touch calibration at the first start and via CONFIG `Touch`; boot gesture removed
- Test builds for touch log and hit map; manual with quick start, glossary and symptom table

### 1.3.1

- Fixes: CLK2 stays off while CH3 is in `ERR`; `~` on MAIN right after boot; integer VCO limits with ±50 ppm; reconnect in the same supervision cycle
- Saving only on a real change and retried every 30 s; stricter frequency parser
- Restart instead of freezing on an LVGL assert or a blocked `loop()` (task watchdog)
- `-Wall -Wextra` without warnings, dependencies pinned, CI

### 1.3.0

- SDA/SCL in either order, SI5351 identity check, new connection messages
- One firmware for both boards (display detection)
- CONFIG options `Tuning`, `Spread CH1`, `Off state`; CONFIG scrolls and has an info card; backlight in 10 % steps
- Actual output frequency on the channel pages; divider planning host-tested

### 1.2.0 and older

- 1.2.0: own SI5351 register driver (exact correction, tuning without PLL reset), supervision with reconnect, outputs start off, tap a digit to select the step
- 1.1.0: layout without clipping, tappable MAIN cards with switches, step presets and hold-to-repeat, numpad, frequency correction, CH3 IQ from 4.730 MHz

## Pinout

The authoritative pin table is in [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md), section 2.

| Function | Bus | GPIO |
|---|---:|---:|
| TFT MOSI / MISO / SCK / CS | HSPI | 13 / 12 / 14 / 15 |
| TFT DC / RST | - | 2 / -1 (EN) |
| Touch MOSI / MISO / SCK / CS | VSPI | 32 / 39 / 25 / 33 |
| Touch IRQ | - | 36 |
| SI5351 SDA / SCL (CN1, either order) | I2C | 22 / 27 |
| Backlight PWM | LEDC | 21 |
| RGB LED R / G / B | active low | 4 / 16 / 17 |
| Free input only | - | 35 |

The firmware tries SDA=22 first, then the swapped assignment; the log shows the result, e.g. `[APP] SI5351 at 0x60, SDA=27 SCL=22 (swapped)`.

## Libraries

Pinned in `platformio.ini`: Arduino-ESP32 via `espressif32@6.7.0`, `bodmer/TFT_eSPI@2.5.43` (exact: the ST7789 init sequence is copied from it), `lvgl/lvgl@8.4.0`, XPT2046_Touchscreen at commit `d57f64c` (tag v1.4), scpi-parser at commit `8866159` (v2.3 + build fixes), `Preferences` from Arduino-ESP32.

## Build and Flash

| Board | Display controller | Detected by (RDDID, command 04h) |
|---|---|---|
| Original CYD, micro-USB only | ILI9341 | ID `00 00 00` |
| CYD2USB, additional USB-C port | ST7789 type | any other ID (measured `81 81 B3`) |

```bash
pio run -t upload            # both boards
pio device monitor -b 115200
```

The controller is read over SPI before the display init at every boot; nothing is stored. For an unrecognized board (display shows only noise), build with `-DDISPLAY_FORCE_PANEL=1` (ILI9341) or `2` (ST7789). `tools/build_info.py` passes branch, commit (`+` = uncommitted changes) and build time to `src/utils/build_info.cpp` only. Partition table `huge_app.csv`; a standard 4 MB module is sufficient.

## Tests

```bash
pio test -e native-win         # host tests on Windows (MinGW bundled with PlatformIO)
pio test -e native             # host tests on Linux/macOS/CI (gcc in PATH)
pio test -e native-win-fault   # IQ error state (TEST_FAULT_IQ); native-fault on Linux
```

Host tests cover the SI5351 driver and planner against a register model (`test/helpers/si5351_model.h`, decoded independently of the driver), the AppController with settings and supervision, display detection against a panel model, touch filter and calibration, GUI input logic, status texts, the color contrast, the remote command queue and the SCPI command tree with its line framer. `test/fakes` replaces the Arduino core, `Wire` (with fault injection, swapped wiring, line held low, foreign device) and `Preferences`.

Touch test builds for the measurement plan (not built by a plain `pio run`, never released; CI builds the hit map env):

```bash
pio run -e esp32-2432S028R-touchlog -t upload   # normal GUI, one [TOUCH] log line per touch
pio run -e esp32-2432S028R-hitmap -t upload     # 20 target crosses, [HITMAP] and [TOUCH] lines
```

## Notes and Limitations

- CH3 shares the PLL of its master (CH1 = PLL A, CH2 = PLL B); IQ pairs use an integer MultiSynth divider and a PLL reset to align the phases. Below 4.730 MHz CH3 waits and starts automatically.
- Spread spectrum exists only on PLL A and not in `Integer` mode; in `Integer` the frequency lies on a raster (7.1 MHz → 7.102 273 MHz).
- Drive strength is one value for all outputs.
- Switching artifacts are allowed by design; `Fractional` retunes standalone outputs without a PLL reset.

## Architecture

- `main.cpp`: initialization order and loop
- `core/app_controller.*`: single source of truth for runtime state, the only user of the SI5351 driver
- `drivers/si5351_manager.*`, `drivers/si5351_plan.*`: SI5351 register driver and PLL/MultiSynth planning per tuning mode (pure functions)
- `drivers/display_detect.*`, `drivers/display_panel.*`: controller detection (host-testable) and TFT_eSPI with the ST7789 init
- `drivers/touch*`, `drivers/backlight.*`, `drivers/status_led.*`: touch with filter, backlight PWM, RGB LED
- `hal/i2c_bus.*`: owns `Wire`, opens either SDA/SCL assignment
- `services/*`: Preferences storage and autosave
- `remote/*`: command queue and state snapshot for remote access, line framer, SCPI command tree (`scpi.*`) and the USB link (`scpi_usb.*`)
- `net/*`: network settings (`net_settings`), WiFi policy (pure) and manager, SHA-1/PBKDF2
- `gui/gui.cpp`, `gui/gui_layout.h`, `gui/pages/*`: LVGL setup, navigation and dialogs, layout grid and colors, pages
- `utils/*`: pure, host-tested logic (`frequency_utils`, `input_logic`, `message_queue`, `status_text`, `contrast`, `touch_cal`, `touch_test`), logger, build info
