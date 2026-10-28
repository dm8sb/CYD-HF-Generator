# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

PlatformIO + Arduino-ESP32 firmware (LVGL 8.3 GUI) for a 3-channel SI5351 HF square-wave generator on the ESP32-2432S028R "Cheap Yellow Display". See README.md for pinout and features, PROJECT_SPECIFICATION.md for the original spec, USER_MANUAL_DE/EN.md for operation.

## Commands

```bash
pio run                                   # build default env (original CYD, ILI9341)
pio run -e esp32-2432S028R-usbc           # CYD2USB variant (ST7789)
pio run -e esp32-2432S028R -t upload      # flash; wrong variant = display noise only
pio device monitor -b 115200

pio test -e native-win                    # host unit tests on Windows (bundled MinGW)
pio test -e native                        # host unit tests on Linux/macOS (gcc in PATH)
pio test -e native-win -f test_si5351     # run a single test suite (test/test_<name>/)
```

The ESP32 envs set `test_ignore = *`; tests only run in the native envs.

## Architecture

- `src/main.cpp` defines init order and the loop. `core/app_controller.*` is the single source of truth for runtime state (frequencies, output enables, CH3 IQ follower, drive strength, correction) and the only thing that talks to the SI5351 driver. GUI pages read state from and dispatch actions to the controller; they must not touch drivers directly.
- `drivers/si5351_manager.*` is a self-written register driver (not a library): PLL/MultiSynth math per AN619, glitch-free retuning without PLL reset for standalone outputs, ppm correction, supervision/reconnect. CH1 uses PLL A, CH2 PLL B; CH3 shares its master's PLL. CH3 IQ needs master ≥ 4.730 MHz (VCO ≥ 600 MHz, 7-bit phase register); 90° via phase register, 180/270° additionally via CLK_INV. Below that CH3 waits and auto-starts.
- `services/settings.*` + `storage.*`: Preferences/NVS persistence with 5 s autosave debounce. Outputs always start off at power-up.
- `gui/gui.cpp`: LVGL setup, deferred navigation, shared widgets, action dispatch, queued message boxes. `gui/gui_layout.h` holds the layout grid and colors (layout must stay free of clipping/scrollbars at 320x240). Pages in `gui/pages/`.
- `include/config.h` (e.g. `LOG_DEBUG`), `pins.h`, `constants.h`, `types.h`. TFT_eSPI is configured entirely via build flags in `platformio.ini` (`USER_SETUP_LOADED`), not a User_Setup.h. Touch (XPT2046) is on a separate VSPI bus with fixed default calibration.
- SI5351 at 0x60 on CN1 (GPIO22/27) with SDA/SCL in either order: `hal/i2c_bus.*` owns `Wire` and opens one assignment (`I2CMap`); `AppController::connect_()` tries both at boot and every 2 s while the chip is not ready, and `SI5351Manager::begin()` checks the identity before writing. Never call `Wire` without `I2CBus::isOpen()` — in arduino-esp32 2.0.x a transfer on a closed bus leaks the Wire lock and the next one deadlocks.

## Host tests

The native envs compile only hardware-independent sources (see `build_src_filter` in `[native_test]`: app_controller, backlight, si5351_manager, hal/i2c_bus, services, frequency_utils, logger) with `-std=gnu++11`. Keep those files free of LVGL/TFT dependencies. `test/fakes/` replaces Arduino, `Wire` (an SI5351 register model with fault injection, straight/swapped wiring, line held low, foreign device; `Fake::wireMisuse()` flags transfers on a closed bus) and `Preferences`. `test/helpers/si5351_model.h` decodes written registers back into frequencies independently of the driver's arithmetic — use it to verify driver changes rather than reusing driver math.
