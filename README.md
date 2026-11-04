# CYD HF Generator

PlatformIO + Arduino-ESP32 firmware for an ESP32-2432S028R / Sunton Cheap Yellow Display based 3-channel HF square-wave generator using an SI5351.

## Documentation

- User manual (reference for all behavior): [USER_MANUAL_DE.md](USER_MANUAL_DE.md), German
- English translation: [USER_MANUAL_EN.md](USER_MANUAL_EN.md)
- Requirements and decisions: [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md)
- Architecture and development rules: [CLAUDE.md](CLAUDE.md)

## Features

- ILI9341 (original CYD) or ST7789 (CYD2USB) 320x240 landscape GUI with LVGL; one firmware, the display controller is detected at boot
- XPT2046 touch on a separate VSPI bus
- SI5351 clock generator on CN1 (GPIO22 / GPIO27); SDA and SCL are detected automatically, the wires may be swapped
- CH1 and CH2 independent 1-100 MHz outputs; the channel pages show the actual output frequency
- CH3 IQ follower of CH1 or CH2 with selectable 0/90/180/270 degree phase setting (4.730-100 MHz, 180/270 via output inversion)
- Independent output enable per channel; switched-off outputs Low, High or High-Z (CONFIG `Off state`)
- Tuning modes `Fractional` (finest steps, no dropouts), `Low noise` (integer MultiSynth) and `Integer` (integer PLL and MultiSynth, frequency on a raster)
- Optional spread spectrum on CH1 (PLL A): -0.5 %, -1.5 %, +/-0.5 %
- 2/4/6/8 mA drive strength for all outputs
- Backlight PWM on GPIO21 only, 10-100 % in 10 % steps
- Preferences/NVS persistence with 5 second autosave debounce
- CONFIG info card with firmware version, git branch/commit, build time, display controller and SI5351 wiring
- Serial logging for boot (with version and build info), I2C, SI5351, display, touch, output and settings events (frequency changes and verbose SI5351 logs with `LOG_DEBUG 1` in `include/config.h`)

## Changes in 1.1.0

- Layout fixed: no clipped or overlapping controls, no scrollbars (header padding, card sizes, layout grid in `gui/gui_layout.h`)
- Main page cards are tappable and have their own ON/OFF switch; status ON / OFF / WAIT / NO HW
- Step presets (1 Hz-1 MHz) on the channel pages, hold-to-repeat for +/-, SI5351 deviation shown
- New numpad (4x4, back button, input validation instead of silent clamping)
- Frequency correction in ppm (0.01/0.1/1 ppm steps), no SI5351 re-init on change
- CH3 IQ only from 4.730 MHz (VCO >= 600 MHz), all phases in the whole range, CH3 stays requested while waiting
- Only the affected PLL is reset on frequency changes; backlight minimum 5 %; error dialogs close with OK and do not stack

## Changes in 1.2.0

- All outputs start off after power-up
- Own SI5351 register driver: exact frequency correction, glitch-free tuning (no PLL reset), ≤ 0.5 Hz error, unused MultiSynth powered down
- SI5351 supervision with automatic reconnect
- Backlight switches on after the first frame (no noise at boot)
- Dialogs for NVS errors; dialogs are queued
- Channel page: tap a digit to select the step, tap the unit to open the numpad

## Changes in 1.3.0

- SDA/SCL at CN1 in either order: the firmware tries both assignments at boot and on every reconnect (hot plug, replugging during operation); no pin setting in `pins.h` any more
- SI5351 identity check (registers 0, 183, read-back of register 3): another device at 0x60 is not programmed
- New connection messages (`SI5351 not found`, `I2C line stays low`, `not an SI5351`); connection dialogs close by themselves after reconnecting
- Detection at 100 kHz, then 400 kHz; stays at 100 kHz if the bus does not work at 400 kHz (weak pull-ups)
- Boot no longer hangs after a failed I2C init; a line held low by the SI5351 after a reset is released at boot
- Bus scan only with `LOG_DEBUG`, `CORE_DEBUG_LEVEL=1`
- One firmware for both boards: the display controller (ILI9341 / ST7789) is detected at boot; boot gesture as fallback; the `esp32-2432S028R-usbc` environment is gone
- CONFIG page scrolls (all other pages stay fixed); the save state moved into the CONFIG header
- Backlight with `-`/`+` in 10 % steps, 10-100 % (was a slider, 5-100 %); older stored values are rounded
- New CONFIG options, stored in NVS; without them (settings from 1.2.0) the 1.2.0 behavior applies:
  - `Tuning`: `Fractional` (default, as before), `Low noise`, `Integer`
  - `Spread CH1`: `Off` (default), `-0.5 %`, `-1.5 %`, `+/-0.5 %`; written explicitly at init, so a module with programmed NVM cannot start with spread spectrum on
  - `Off state`: `Low` (default, as before), `High`, `High-Z`; register 24 is written at init
- Channel and CH3 pages show the actual output frequency (`Out 10.000 000 MHz (+0.3 Hz)`) instead of step and deviation; MAIN marks a rastered frequency in `Integer` mode with `~`
- CONFIG info card and boot log line with version, branch, commit and build time (`tools/build_info.py`)
- Divider planning as pure functions (`drivers/si5351_plan.*`), host-tested
- Removed unused code and the per-channel drive fields; frequency changes are logged only with `LOG_DEBUG`; TFT_eSPI pinned to exactly 2.5.43

## Pinout

The authoritative pin table is in [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md), section 2.

| Function | Bus | GPIO |
|---|---:|---:|
| TFT MOSI | HSPI | 13 |
| TFT MISO | HSPI | 12 |
| TFT SCK | HSPI | 14 |
| TFT CS | HSPI | 15 |
| TFT DC | - | 2 |
| TFT RST | - | -1 / EN |
| Touch MOSI | VSPI | 32 |
| Touch MISO | VSPI | 39 |
| Touch SCK | VSPI | 25 |
| Touch CS | VSPI | 33 |
| Touch IRQ | - | 36 |
| SI5351 SDA / SCL (CN1, either order) | I2C | 22 / 27 |
| Backlight PWM | LEDC | 21 |
| RGB LED R | active low | 4 |
| RGB LED G | active low | 16 |
| RGB LED B | active low | 17 |
| Free input only | - | 35 |

Note: the original hardware specification listed SDA=27/SCL=22, the verified device uses SDA=22/SCL=27. The firmware tries SDA=22 first and then the swapped assignment, so both wirings work. The serial log shows the result, e.g. `[APP] SI5351 at 0x60, SDA=27 SCL=22 (swapped)`.

## Libraries

Pinned in `platformio.ini`:

- Arduino-ESP32 via `espressif32@6.7.0`
- `bodmer/TFT_eSPI@2.5.43` (exact: the ST7789 init sequence is copied from it)
- `lvgl/lvgl@8.3.11`
- XPT2046_Touchscreen from `https://github.com/PaulStoffregen/XPT2046_Touchscreen.git`, pinned to commit `d57f64c` (tag v1.4)
- `Preferences` from Arduino-ESP32

## Build and Flash

Two board variants are sold as ESP32-2432S028R. They differ only in the display controller, and the same firmware runs on both:

| Board | Display controller | Detected by (RDDID, command 04h) |
|---|---|---|
| Original CYD, micro-USB only | ILI9341 | ID `00 00 00` |
| CYD2USB, additional USB-C port | ST7789 type | any other ID (measured `81 81 B3`, datasheet `85 85 52`) |

```bash
cd CYD-HF-Generator
pio run -t upload            # both boards
pio device monitor -b 115200
```

At boot the firmware reads the controller over SPI (MISO GPIO12) before the display is initialized and logs the result, e.g. `[DISPLAY] ST7789 (detected); detected ST7789: ID 8181B3, ...`. If the display still shows only noise, hold the touch screen while switching on until the backlight blinks twice (boot gesture): the firmware uses the other controller and stores the choice; a second gesture returns to automatic detection. Developers can skip the detection with `-DDISPLAY_FORCE_PANEL=1` (ILI9341) or `2` (ST7789).

The pre-build script `tools/build_info.py` takes branch, commit and build time from git (`unknown` without git, `+` after the commit for uncommitted changes); only `src/utils/build_info.cpp` is recompiled for it.

The project uses `huge_app.csv`. A standard 4 MB ESP32 module is sufficient.

## Unit Tests

Host tests run without hardware: frequency helpers, the SI5351 driver and its divider planner against a register model of the chip, the AppController with settings, supervision and CH3 IQ logic, and the display detection against a panel model (`test/helpers/panel_model.h`, read behavior as measured on both boards).

```bash
pio test -e native-win   # Windows, uses the MinGW toolchain bundled with PlatformIO
pio test -e native       # Linux/macOS/CI, needs gcc in PATH
```

`test/fakes` replaces the Arduino core, `Wire` (SI5351 register model with fault injection, straight or swapped wiring, line held low, foreign device at 0x60) and `Preferences`. `test/helpers/si5351_model.h` decodes the written registers back into frequencies (AN619), independently of the driver's own arithmetic.

## Operation

See the user manual for details.

- Main page shows all channel states; tap a card to open the channel, use its switch to toggle the output.
- CH1/CH2 pages allow step preset selection, +/- adjustment (hold to repeat), ON/OFF, and numpad entry, and show the actual output frequency.
- Numpad accepts max 9 characters; a unit key (Hz/kHz/MHz) confirms, the back key cancels. Input outside 1-100 MHz is rejected.
- CH3 page selects master, phase and follower enable.
- Config page (scrollable) sets backlight (10-100 %), frequency step, SI5351 correction in ppm, one global drive strength for all channels, tuning mode, spread spectrum on CH1 and the off state, and shows firmware, build, display and SI5351 info.

## Notes and Limitations

- CH3 always shares the PLL of its master (CH1 = PLL A, CH2 = PLL B). The driver configures master and CH3 as an IQ pair with an integer MultiSynth divider and resets only the affected PLL to align the phases.
- The SI5351 phase register is 7-bit. 90 degrees uses the register (= MultiSynth divider, <= 127 in the IQ range); 180/270 degrees invert the CH3 output (CLK_INV) in addition, so every phase works from 4.730 to 100 MHz.
- Touch calibration UI and runtime calibration code are disabled. The firmware always uses the built-in default XPT2046 calibration values: x_min=300, x_max=3800, y_min=300, y_max=3800.
- Drive strength is a single global CONFIG value for CH1, CH2 and CH3; CH3 has no independent drive setting.
- Below 4.730 MHz CH3 waits (output off, switch stays ON) and starts automatically once the master frequency allows IQ mode.
- Switching artifacts are allowed by design. In `Fractional` mode standalone outputs are retuned without a PLL reset; IQ pairs are disabled, reconfigured and realigned.
- Spread spectrum exists only on PLL A (CH1, and CH3 with master CH1) and is not available in `Integer` mode.
- In `Integer` mode the frequency lies on a raster (e.g. 7.1 MHz → 7.102 273 MHz); the correction only shifts the displayed actual frequency.

## Architecture

The project follows a modular structure:

- `main.cpp`: initialization order and loop
- `core/app_controller.*`: single source of truth for runtime state and SI5351 coordination
- `drivers/si5351_manager.*`: SI5351 register driver (register cache, PLL reset only on change, supervision, spread spectrum, off state)
- `drivers/si5351_plan.*`: PLL / MultiSynth planning per tuning mode as pure functions, host-tested
- `drivers/*`: display (`display_detect.*`: controller detection, host-testable; `display_panel.*`: TFT_eSPI, ST7789 init), touch, backlight and RGB status LED drivers
- `services/*`: Preferences storage and autosave
- `hal/i2c_bus.*`: owns `Wire`; opens either SDA/SCL assignment, bus release at boot; the detection itself runs in `AppController::connect_()`
- `gui/gui.cpp`: LVGL setup, deferred navigation, shared widgets, action dispatch and message box
- `gui/gui_layout.h`: layout grid and color scheme
- `gui/pages/*`: page creation and refresh for Main, channel (CH1/CH2), CH3, Config and Numpad pages
- `utils/build_info.*`: version, branch, commit and build time
- `utils/*`: logging and frequency helpers
- `tools/build_info.py`: PlatformIO pre-build script; passes the git information only to `build_info.cpp`
