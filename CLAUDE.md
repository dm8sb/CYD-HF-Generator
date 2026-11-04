# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

PlatformIO + Arduino-ESP32 firmware (LVGL 8.3 GUI) for a 3-channel SI5351 HF square-wave generator on the ESP32-2432S028R "Cheap Yellow Display". See README.md for pinout, features and changes, PROJECT_SPECIFICATION.md for requirements and decisions, USER_MANUAL_DE.md for all behavior.

## Commands

```bash
pio run                                   # build the firmware (both boards)
pio run -t upload --upload-port COMx      # flash; display controller is detected at boot
pio device monitor -b 115200

pio test -e native-win                    # host unit tests on Windows (bundled MinGW)
pio test -e native                        # host unit tests on Linux/macOS (gcc in PATH)
pio test -e native-win -f test_si5351     # run a single test suite (test/test_<name>/)
pio test -e native-win-fault              # IQ error state: test_fault_iq with -DTEST_FAULT_IQ=1 (native-fault on Linux)
```

The ESP32 envs set `test_ignore = *`; tests only run in the native envs.

## Architecture

- `src/main.cpp` defines init order and the loop. `core/app_controller.*` is the single source of truth for runtime state (frequencies, output enables, CH3 IQ follower, drive strength, correction, tuning mode, spread, off state) and the only thing that talks to the SI5351 driver. GUI pages read state from and dispatch actions to the controller; they must not touch drivers directly (the CONFIG info card gets the I2C pins from `AppController`, not from `I2CBus`).
- `drivers/si5351_manager.*` is a self-written register driver (not a library): PLL/MultiSynth math per AN619, glitch-free retuning without PLL reset for standalone outputs (in `Fractional`), ppm correction, supervision/reconnect. CH1 uses PLL A, CH2 PLL B; CH3 shares its master's PLL. CH3 IQ needs master ≥ 4.730 MHz (VCO ≥ 600 MHz, 7-bit phase register); 90° via phase register, 180/270° additionally via CLK_INV. Below that CH3 waits and auto-starts.
- `drivers/si5351_plan.*` holds the PLL/MultiSynth planning per `TuningMode` (`Fractional`: fixed PLL, fractional MultiSynth; `Low noise`: even integer MultiSynth with hysteresis, fractional PLL; `Integer`: integer PLL and MultiSynth on a raster) as pure functions without I2C, so they are host-tested directly. `SI5351Manager` plans first and then writes only the changed registers. Spread spectrum exists only on PLL A and is locked in `Integer`; the off state is register 24, written in `begin()` before any output can be enabled.
- `services/settings.*` + `storage.*`: Preferences/NVS persistence with 5 s autosave debounce. Outputs always start off at power-up.
- `gui/gui.cpp`: LVGL setup, deferred navigation, shared widgets, action dispatch, queued message boxes. `gui/gui_layout.h` holds the layout grid and colors (no clipping or overlap at 320x240). Pages do not scroll; the only exception is the CONFIG content area below its fixed header (snaps to rows, `Gui` keeps the scroll position in RAM). Pages in `gui/pages/`; their input logic (numpad, digit tap, step markup) lives LVGL-free in `utils/input_logic.*` and is host-tested.
- Display: one firmware for the original CYD (ILI9341) and the CYD2USB (ST7789). TFT_eSPI is built for the ILI9341; `drivers/display_detect.*` (host-testable) reads the controller ID over SPI before `tft.init()`, `drivers/display_panel.*` owns TFT_eSPI and sends the ST7789 init/MADCTL when needed. GUI code goes through `DisplayPanel`, never `TFT_eSPI::setRotation()`. The CYD2USB drives MISO so weakly that any internal pull on GPIO12 breaks the read-back.
- `include/config.h` (`APP_VERSION`, `LOG_DEBUG`, `DISPLAY_FORCE_PANEL`), `pins.h`, `constants.h`, `types.h`. `tools/build_info.py` (`extra_scripts = pre:`) passes git branch, commit (`+` = dirty) and build time only to `src/utils/build_info.cpp` via `env.AddBuildMiddleware`, so a build recompiles just that file; don't use `__DATE__`/`__TIME__` or add these defines globally. `build_info.cpp` is not part of the host tests. TFT_eSPI is configured entirely via build flags in `platformio.ini` (`USER_SETUP_LOADED`), not a User_Setup.h. Touch (XPT2046) is on a separate VSPI bus with fixed default calibration.
- SI5351 at 0x60 on CN1 (GPIO22/27) with SDA/SCL in either order: `hal/i2c_bus.*` owns `Wire` and opens one assignment (`I2CMap`); `AppController::connect_()` tries both at boot and every 2 s while the chip is not ready, and `SI5351Manager::begin()` checks the identity before writing. Never call `Wire` without `I2CBus::isOpen()` — in arduino-esp32 2.0.x a transfer on a closed bus leaks the Wire lock and the next one deadlocks.

## Host tests

The native envs compile only hardware-independent sources (see `build_src_filter` in `[native_test]`: app_controller, backlight, display_detect, si5351_manager, si5351_plan, hal/i2c_bus, services, frequency_utils, input_logic, logger) with `-std=gnu++11`. Keep those files free of LVGL/TFT dependencies. `test/fakes/` replaces Arduino, `Wire` (an SI5351 register model with fault injection, straight/swapped wiring, line held low, foreign device; `Fake::wireMisuse()` flags transfers on a closed bus) and `Preferences`. `test/helpers/si5351_model.h` decodes written registers back into frequencies independently of the driver's arithmetic — use it to verify driver changes rather than reusing driver math.

## Documentation

- `PROJECT_SPECIFICATION.md` holds only requirements (`A-n`) and decisions (`E-n`, dated, with rationale and measured values). No behavior descriptions, page layouts, log lines, init order or class responsibilities there.
- All behavior (operation, pages, messages, log lines, limits, examples) is described only in `USER_MANUAL_DE.md`. `USER_MANUAL_EN.md` is its translation with the same structure and tables and no own content; change DE first and update EN in the same commit.
- README: overview, build, pinout, changes per version. A new decision gets the next `E-n` number in the specification.
- `ARBEITSPLAN.md` is the work order: summary of audit, WEB-HMI concept and todo, plus the numbered step list. **Every step ends with one commit** (gate first: tests green, 0 warnings, manuals DE+EN, specification, README and todo in the same commit; tick the step in the same commit). Commits stay local; push, merge to `main` and publishing tags only on explicit request.
- `MESSPLAN.md` is the hardware measurement plan (instruments, set-ups, 150 measurements with steps, evaluation). `todo.md` is the backlog; `AUDIT_2026-11.md` and `KONZEPT_WEB_HMI.md` are the sources for findings and the WEB-HMI design.
