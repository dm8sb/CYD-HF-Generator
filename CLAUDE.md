# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

PlatformIO + Arduino-ESP32 firmware (LVGL 8.4 GUI) for a 3-channel SI5351 HF square-wave generator on the ESP32-2432S028R "Cheap Yellow Display". See README.md for pinout, features and changes, PROJECT_SPECIFICATION.md for requirements and decisions, USER_MANUAL_DE.md for all behavior.

## Commands

```bash
pio run                                   # build the firmware (both boards)
pio run -t upload --upload-port COMx      # flash; display controller is detected at boot
pio device monitor -b 115200

pio test -e native-win                    # host unit tests on Windows (bundled MinGW)
pio test -e native                        # host unit tests on Linux/macOS (gcc in PATH)
pio test -e native-win -f test_si5351     # run a single test suite (test/test_<name>/)
pio test -e native-win-fault              # IQ error state: test_fault_iq with -DTEST_FAULT_IQ=1 (native-fault on Linux)
pio test -e native-cpp20                  # host tests in C++20 like the firmware on Arduino 3.x (Linux/CI; MinGW is GCC 5.1)
pio run -e esp32-2432S028R-touchlog -t upload --upload-port COMx   # test build: one log line per touch
pio run -e esp32-2432S028R-hitmap -t upload --upload-port COMx     # test build: hit map + touch log
```

The ESP32 envs set `test_ignore = *`; tests only run in the native envs.

## Architecture

- `src/main.cpp` defines init order and the loop. `core/app_controller.*` is the single source of truth for runtime state (frequencies, output enables, CH3 IQ follower, drive strength, correction, tuning mode, spread, off state) and the only thing that talks to the SI5351 driver. GUI pages read state from and dispatch actions to the controller; they must not touch drivers directly (the CONFIG info card gets the I2C pins from `AppController`, not from `I2CBus`).
- `drivers/si5351_manager.*` is a self-written register driver (not a library): PLL/MultiSynth math per AN619, glitch-free retuning without PLL reset for standalone outputs (in `Fractional`), ppm correction, supervision/reconnect. CH1 uses PLL A, CH2 PLL B; CH3 shares its master's PLL. CH3 IQ needs master ≥ 4.730 MHz (VCO ≥ 600 MHz, 7-bit phase register); 90° via phase register, 180/270° additionally via CLK_INV. Below that CH3 waits and auto-starts.
- `drivers/si5351_plan.*` holds the PLL/MultiSynth planning per `TuningMode` (`Fractional`: fixed PLL, fractional MultiSynth; `Low noise`: even integer MultiSynth with hysteresis, fractional PLL; `Integer`: integer PLL and MultiSynth on a raster) as pure functions without I2C, so they are host-tested directly. `SI5351Manager` plans first and then writes only the changed registers. Spread spectrum exists only on PLL A and is locked in `Integer`; the off state is register 24, written in `begin()` before any output can be enabled.
- `services/settings.*` + `storage.*`: Preferences/NVS persistence with 5 s autosave debounce. Outputs always start off at power-up.
- `gui/gui.cpp`: LVGL setup, deferred navigation, shared widgets, action dispatch, queued message boxes. `gui/gui_layout.h` holds the layout grid and colors (no clipping or overlap at 320x240). Pages do not scroll; the only exception is the CONFIG content area below its fixed header (snaps to rows, `Gui` keeps the scroll position in RAM). Pages in `gui/pages/`; their input logic (numpad, digit tap, step markup) and the dialog queue live LVGL-free in `utils/input_logic.*` and `utils/message_queue.*` and are host-tested; timing constants are in `constants.h`.
- Display: one firmware for the original CYD (ILI9341) and the CYD2USB (ST7789). TFT_eSPI is built for the ILI9341; `drivers/display_detect.*` (host-testable) reads the controller ID over SPI before `tft.init()`, `drivers/display_panel.*` owns TFT_eSPI and sends the ST7789 init/MADCTL when needed. GUI code goes through `DisplayPanel`, never `TFT_eSPI::setRotation()`. The CYD2USB drives MISO so weakly that any internal pull on GPIO12 breaks the read-back.
- `include/config.h` (`APP_VERSION`, `LOG_DEBUG`, `DISPLAY_FORCE_PANEL`), `pins.h`, `constants.h`, `types.h`, `palette.h` (colors as hex; `test_contrast` checks the pairs the pages use, add new pairs there). `tools/build_info.py` (`extra_scripts = pre:`) passes git branch, commit (`+` = dirty) and build time only to `src/utils/build_info.cpp` via `env.AddBuildMiddleware`, so a build recompiles just that file; don't use `__DATE__`/`__TIME__` or add these defines globally. `build_info.cpp` is not part of the host tests. TFT_eSPI is configured entirely via build flags in `platformio.ini` (`USER_SETUP_LOADED`), not a User_Setup.h. Touch (XPT2046) is on a separate VSPI bus; its calibration per board comes from the wizard (`utils/touch_cal.*`, host-tested; NVS keys `tc_*`), `TOUCH_CAL_DEFAULT` without one.
- Remote access (WEB-HMI groundwork): `remote/command.*` queues `Command`s from network tasks (bounded, results by ticket) and `loop()` executes them through `execute()`, which sets the event source; network tasks read only the `StateSnapshot` (seqlock). They never call `AppController`, LVGL, I2C or NVS directly. Setters return `Result` and never clamp (only `adjust*`). Sources are `Touch`, `Web`, `Scpi`, `Usb`, `Internal` (`isRemote()`); `Remote outputs: local only` makes remote switch-on `Denied`; the queue limits each source to 20 writes/s. SCPI: `remote/line_framer.*` (pure) splits lines, `remote/scpi.*` holds the command tree on j123b567/scpi-parser (configured by `include/scpi_user_config.h`, numbers parsed exactly in `ScpiNumber`, not by the library), `remote/scpi_usb.*` runs a session in `loop()` (`LoopScpiBackend`: queue, drain, publish) and switches the logger to the ring buffer while a USB session is active.
- WiFi: `net/net_settings.*` (NVS namespace `hfnet`, validation; keys stored as passphrase, E-30) and `net/wifi_policy.*` are pure and host-tested; `net/wifi_manager.*` runs in `loop()` (the GUI gets it through `Gui::setWifi()` for status, scan and settings; WIFI pages in `gui/pages/page_wifi.cpp`; foreign strings only via `utils/sanitize.*`). The LVGL pool comes from the heap (`LV_MEM_POOL_ALLOC`, E-31): the static DRAM is full with the WiFi stack, so new large buffers go to the heap too. (WiFi events only set flags, no `WiFiMulti::run()`, which blocks). Test builds with credentials: `-DTEST_WIFI=1` plus the git-ignored `include/test_wifi_local.h`; never commit credentials.
- SI5351 at 0x60 on CN1 (GPIO22/27) with SDA/SCL in either order: `hal/i2c_bus.*` owns `Wire` and opens one assignment (`I2CMap`); `AppController::connect_()` tries both at boot and every 2 s while the chip is not ready, and `SI5351Manager::begin()` checks the identity before writing. `Wire` is called only in `hal/i2c_bus.cpp` (`probe`, `writeRegs`, `readRegs`, all checking `isOpen()`): in arduino-esp32 2.0.x and 3.x a transfer on a closed bus keeps the Wire lock and the next `begin()`/`end()` deadlocks. `test_i2c_bus` (`test_core_*`) pins down the assumed core behavior.

## Host tests

The native envs compile only hardware-independent sources (see `build_src_filter` in `[native_test]`: app_controller, backlight, display_detect, touch_filter, si5351_manager, si5351_plan, hal/i2c_bus, net/net_settings, net/pbkdf2, net/wifi_policy, remote (with scpi-parser from `lib_deps`), services, contrast, frequency_utils, input_logic, message_queue, status_text, touch_cal, touch_test, sanitize, logger) with `-std=gnu++11`, in `native-cpp20` with `-std=gnu++20` (Arduino 3.x builds the firmware as gnu++2a). Keep those files free of LVGL/TFT dependencies. `test/fakes/` replaces Arduino, `Wire` (an SI5351 register model with fault injection, straight/swapped wiring, line held low, foreign device; `Fake::wireMisuse()` flags transfers on a closed bus) and `Preferences`. `test/helpers/si5351_model.h` decodes written registers back into frequencies independently of the driver's arithmetic — use it to verify driver changes rather than reusing driver math.

## Documentation

- `PROJECT_SPECIFICATION.md` holds only requirements (`A-n`) and decisions (`E-n`, dated, with rationale and measured values). No behavior descriptions, page layouts, log lines, init order or class responsibilities there.
- All behavior (operation, pages, messages, log lines, limits, examples) is described only in `USER_MANUAL_DE.md`. `USER_MANUAL_EN.md` is its translation with the same structure and tables and no own content; change DE first and update EN in the same commit.
- README: overview, build, pinout, changes per version. A new decision gets the next `E-n` number in the specification.
- `ARBEITSPLAN.md` is the only planning file: open findings (audit IDs C/S/U/P/T stay valid as names), the WEB-HMI concept (W-E decisions, W-S security requirements, SCPI tree, REST), decisions and the numbered step list. It replaced `todo.md`, `AUDIT_2026-11.md` and `KONZEPT_WEB_HMI.md` (in the git history up to `ee2b91a`). **Every step ends with one commit** (gate first: tests green, 0 warnings, manuals DE+EN, specification, README and ARBEITSPLAN in the same commit; tick the step in the same commit). For UI changes, flash and let the user check on the device before committing. Push the working branch after every commit so no intermediate state is lost (user, 2026-11-06); merge to `main` and publishing tags only on explicit request.
- `MESSPLAN.md` is the hardware measurement plan, ordered by priority (tier 1 = necessary manual measurements first); measurement IDs (A1, K2 …) are stable.
