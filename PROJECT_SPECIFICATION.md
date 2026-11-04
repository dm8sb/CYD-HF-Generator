# CYD HF Generator – Project Specification

Firmware 1.3.0 · Status 2026-10-28

This document contains only **requirements** (A-n, numbered and testable) and **decisions** (E-n, dated, with rationale and measured values where they exist). Behavior as seen by the user (pages, controls, messages, log lines, examples) is described only in [USER_MANUAL_DE.md](USER_MANUAL_DE.md); [USER_MANUAL_EN.md](USER_MANUAL_EN.md) is its translation. Build instructions and the change history are in [README.md](README.md), architecture and development rules in [CLAUDE.md](CLAUDE.md).

---

## 1. Purpose and Use

A touchscreen-controlled 3-channel HF square-wave generator on the ESP32-2432S028R ("Cheap Yellow Display") with an SI5351 clock generator. It is used for experiments with HF mixers and for amateur radio training; low spurs and phase noise and an accurate IQ output matter more than feature count.

| Channel | SI5351 output | Function | PLL |
|---|---|---|---|
| CH1 | CLK0 | independent frequency | A |
| CH2 | CLK1 | independent frequency | B |
| CH3 | CLK2 | IQ follower of CH1 or CH2, phase 0/90/180/270° | PLL of its master |

Out of scope: WiFi, OTA, external buttons, waveforms other than square wave.

---

## 2. Hardware and Pinout

### 2.1 Board variants

Two boards are sold as ESP32-2432S028R. They differ only in the display controller; pinout, touch, backlight and CN1 are identical. One firmware serves both (E-18).

| Board | Recognized by | Display controller | RDDID (04h), measured |
|---|---|---|---|
| Original CYD | micro-USB port only | ILI9341 | `00 00 00` |
| CYD2USB | additional USB-C port | ST7789 type | `81 81 B3` (datasheet ST7789V: `85 85 52`) |

### 2.2 Pin table

This table is authoritative; README and manual repeat parts of it.

| Function | Bus | GPIO | Defined in |
|---|---|---:|---|
| TFT MOSI | HSPI | 13 | build flags (`platformio.ini`) |
| TFT MISO | HSPI | 12 | build flags; no internal pull (E-18) |
| TFT SCK | HSPI | 14 | build flags |
| TFT CS | HSPI | 15 | build flags |
| TFT DC | – | 2 | build flags |
| TFT RST | – | – (EN) | `TFT_RST=-1` |
| Touch MOSI / MISO / SCK | VSPI | 32 / 39 / 25 | `pins.h` |
| Touch CS / IRQ | VSPI / – | 33 / 36 | `pins.h` |
| SI5351 SDA / SCL (CN1, either order) | I2C | 22 / 27 | `pins.h` (`PIN_I2C_A`, `PIN_I2C_B`) |
| Backlight PWM | LEDC | 21 | `pins.h` |
| RGB LED R / G / B (active low) | – | 4 / 16 / 17 | `pins.h` |
| Free, input only | – | 35 | unused |

- GPIO21 is used only for the backlight PWM: never for I2C, buttons, LEDs or other peripherals, and never as an I2C candidate.
- Display and touch use separate SPI buses (HSPI, VSPI).

### 2.3 SI5351 module

- SI5351A with 25 MHz crystal, I2C address 0x60, 3.3 V supply, connected to CN1.
- Crystal load capacitance 10 pF (register 183 = 0xD2).
- Many breakout boards have an active-low `OE` pin: OE = GND enables the outputs.
- The original specification listed SDA = 27 / SCL = 22; the verified wiring is SDA = 22 / SCL = 27. Both work (E-17).
- Clones (e.g. MS5351M) with a different register 183 are accepted with a log warning (E-17).
- Duty cycle and output level at 50 Ω are not yet measured (measurement plan E5, F2 in `todo.md`).

---

## 3. Requirements

### 3.1 Outputs and frequency

- **A-1** CH1 and CH2 generate independent square waves of 1–100 MHz (`FREQ_MIN_HZ`, `FREQ_MAX_HZ`), nominally 50 % duty cycle, settable with 1 Hz resolution.
- **A-2** The requested frequency is stored exactly, not the rounded SI5351 value (E-9). The numpad rejects values outside the range; `+`/`−` clamp to it.
- **A-3** Each output can be switched on and off independently. A switched-off output is disabled via the output enable register and shows the off state selected in CONFIG (A-8). The GUI shows the channel state as ON / OFF (plus ERR, WAIT, NO HW), never the off state.
- **A-4** All outputs are off after power-up; the ON/OFF state is not stored (E-11). After a reconnection of the SI5351 the previous states are restored.
- **A-5** One drive strength (2 / 4 / 6 / 8 mA) applies to all outputs (E-2).
- **A-6** Tuning mode `Fractional` / `Low noise` / `Integer`, global, default `Fractional` (E-19):

  | Mode | MultiSynth | PLL | Tuning |
  |---|---|---|---|
  | Fractional | fractional | fixed 900 or 887.5 MHz | no dropout; the PLL changes (with reset) only where 900 MHz cannot reach ≤ 0.25 Hz |
  | Low noise | even integer (`MS_INT`) | fractional, takes frequency and correction | PLL changes without reset while the divider stays (VCO 600–900 MHz); reset when the divider changes |
  | Integer | even integer | integer, 24–36 × crystal (`FBx_INT` for even ratios, AN619 3.2.1) | nearest raster point crystal × a / MS |

  A mode change reconfigures all outputs; one brief dropout of running outputs is allowed.
- **A-7** Spread spectrum `Off` / `-0.5 %` / `-1.5 %` / `+/-0.5 %` on PLL A only (CH1, and CH3 with master CH1), modulation ≈ 31.5 kHz, default `Off` (E-20). The setting is written explicitly at init (`SSC_EN` = 0 for `Off`). Not available in `Integer`; switching to `Integer` turns it off. Switching it resets PLL A (brief CH1 dropout, IQ realignment).
- **A-8** Off state `Low` / `High` / `High-Z` (register 24, `CLKx_DIS_STATE`), global, default `Low` (E-21). It is written at init right after the identity check, before any output can be enabled; a change applies at once and does not touch running outputs.
- **A-9** A MultiSynth that is not needed is powered down (CLK2 while CH3 is not in IQ mode), unless the off state needs the output driver (E-21).
- **A-10** A PLL is written and reset only when its parameters change. A reset affects only the PLL of the changed channel, never the channel on the other PLL. In `Fractional`, tuning a standalone output neither disables it nor resets its PLL, except for the rare PLL change of A-6.

### 3.2 Accuracy

- **A-11** Correction ±50 ppm in units of 0.01 ppm (`freq_correction`, positive = crystal runs fast). The crystal is modeled as 25 MHz × (1 + c × 10⁻⁸) and all dividers are computed against it with 64-bit integer math, so the correction is exact at every frequency. A change reconfigures the outputs without re-initializing the SI5351.
- **A-12** Maximum deviation of the synthesized from the requested frequency (register rounding only, correction assumed right):

  | Case | Limit | Host test |
  |---|---|---|
  | Standalone, `Fractional` | ≤ 0.5 Hz | ≤ 0.375 Hz at correction 0 |
  | Standalone, `Low noise` | ≤ 1.4 Hz | – |
  | IQ, `Fractional` / `Low noise` | ≤ 1.4 Hz | 1.32 Hz worst case (91 666 668 Hz) |
  | `Integer` | nearest raster point (kHz range possible) | – |

- **A-13** Fractions are the best rational approximation with a 20-bit denominator (continued fractions). VCO 600–900 MHz and the register limits of AN619 are kept. Accepted exception: in `Fractional` the fixed PLL of 36 × crystal exceeds 900 MHz by up to 45 kHz at +50 ppm.
- **A-14** The channel pages show the actual output frequency computed from the programmed registers (`actualHz()`), highlighted from 1 Hz deviation; with spread spectrum the mean frequency. MAIN shows the requested frequency, prefixed with `~` in `Integer` when the deviation is ≥ 1 Hz.

### 3.3 IQ (CH3)

- **A-15** CH3 has the frequency of its master (CH1 or CH2) and shares its PLL. All four phases 0/90/180/270° work over the whole IQ range 4.730–100 MHz (`IQ_MIN_HZ`, `IQ_MAX_HZ`) (E-7).
- **A-16** IQ pair: integer MultiSynth divider ≤ 127 (7-bit phase register; one unit = ¼ VCO period, so 90° = divider); 180°/270° = 0°/90° plus `CLK_INV` on CLK2. In `Fractional` and `Low noise` the divider is min(127, ⌊900 MHz / f⌋) with a fractional PLL; in `Integer` the PLL is integer as well.
- **A-17** Any change of PLL, divider or phase offset of an IQ pair disables both outputs, reconfigures them and resets the shared PLL to align the phases. A change of drive strength or inversion only is written without reset.
- **A-18** CH3 ON is a request (E-8): CLK2 runs only while the master is ON and inside the IQ range (4.730–100 MHz, narrower with spread spectrum on master CH1, see E-20). Otherwise CLK2 stays off without a dialog, the request is kept (WAIT) and CH3 starts automatically once the conditions are met.
- **A-19** If the IQ setup fails while the SI5351 answers, CLK2 is powered down, the master keeps running standalone, CH3 shows ERR and the setup is retried (A-37).

### 3.4 Settings and persistence

- **A-20** Settings are stored in Preferences/NVS, namespace `hfgen`. Every value is validated on load:

  | Key | Type | Content | Default | On load |
  |---|---|---|---|---|
  | `ch1_freq`, `ch2_freq` | U64 | Hz | 10 MHz, 14 MHz | clamped to 1–100 MHz |
  | `ch3_master` | U8 | 1 / 2 | 1 | invalid → default |
  | `ch3_phase` | U32 | 0 / 90 / 180 / 270 | 90 | invalid → default |
  | `backlight` | U8 | % | 80 | < 5 or > 100 → 80, otherwise nearest 10 % step |
  | `delta` | U32 | step in Hz | 1000 | nearest preset 1 Hz … 1 MHz |
  | `corr` | I32 | 0.01 ppm | 0 | clamped to ±5000 |
  | `drv_def` | U8 | mA | 4 | invalid → default |
  | `tune` | U8 | 0 Fractional, 1 Low noise, 2 Integer | 0 | invalid → default |
  | `ssc` | U8 | 0 Off, 1 −0.5 %, 2 −1.5 %, 3 ±0.5 % | 0 | invalid → default; Off in Integer |
  | `off_st` | U8 | 0 Low, 1 High, 2 High-Z | 0 | invalid → default |
  | `disp_panel` | U8 | 0 automatic, 1 ILI9341, 2 ST7789 | 0 | written only by the boot gesture, only when it changes |

- **A-21** Missing keys give the defaults, so NVS contents from 1.2.0 keep the 1.2.0 behavior. Keys `ch1_en` / `ch2_en` / `ch3_en` from 1.1.0 are ignored. Output states, touch calibration and the SDA/SCL assignment are not stored.
- **A-22** Autosave writes all settings 5 s after the last change (`SETTINGS_AUTOSAVE_MS`).
- **A-23** Without NVS the firmware runs with defaults and saves nothing. NVS unavailable at boot and a failed save each produce a dialog and a save state on CONFIG. A failed save keeps the change and is retried every 30 s (`SETTINGS_RETRY_MS`); the dialog appears once per series of failures.

### 3.5 User interface

- **A-24** LVGL 8.3, 320 × 240 landscape, dark theme with colored cards. Pages: MAIN, CH1, CH2, CH3, CONFIG, NUMPAD.
- **A-25** Page coordinates derive from the layout grid in `gui/gui_layout.h` (margin 8 px, header 38 px, card padding 6 px). No control is clipped or overlaps another. Pages do not scroll, except the CONFIG content area below its fixed header (E-22).
- **A-26** Frequently used controls are at least about 36 px high; yellow buttons use dark text.
- **A-27** Colors: CH1 red, CH2 green, CH3 blue, CONFIG yellow, MAIN and back buttons gray. The RGB LED shows the color of the open channel page and is off on all other pages (E-3).
- **A-28** Frequencies are shown in MHz with grouped digits (`10.700 000 MHz`).
- **A-29** MAIN has no title header; it shows one tappable card per channel with state, frequency and its own switch, plus navigation buttons. The channel pages (CH1–CH3) and CONFIG have a header with title and `MAIN` button; channel pages add the ON/OFF switch with a state text `ON` / `OFF` / `ERR` (E-13).
- **A-30** Frequency entry for CH1/CH2: step presets 1 Hz … 1 MHz, `+`/`−` with hold-to-repeat, tap on a digit selects the step, numeric keypad with validation (E-5).
- **A-31** CONFIG contains backlight (10–100 % in 10 % steps, E-23), step, correction (E-6), drive strength, tuning mode, spread spectrum, off state and an info card (firmware version, branch, commit with dirty mark, build time, display controller with its source, SI5351 pins or state). The save state is visible in every scroll position.
- **A-32** Touch: XPT2046 with fixed calibration (E-1), jitter filter and debounce; no calibration UI.
- **A-33** The backlight stays off until the first frame is drawn (the uninitialized panel shows noise).

### 3.6 Robustness and errors

- **A-34** SI5351 errors never crash the firmware; they are logged, shown as a dialog where useful, and leave the outputs in a safe state.
- **A-35** Supervision every 2 s (`HW_CHECK_INTERVAL_MS`) reads back the output enable and CLK control registers. A failed transfer or unexpected content (e.g. after a power interruption) marks the SI5351 as lost (NO HW); the firmware tries to reconnect in the same cycle and then every 2 s, and restores all outputs with their states.
- **A-36** Without SI5351 no output is configured and no repeated errors appear; all settings remain editable and are saved.
- **A-37** An output setup that fails while the SI5351 answers is shown as ERR (MAIN, header, info line), the switch keeps the requested state, and the setup is retried every 2 s without further dialogs (E-12).
- **A-38** Dialogs: at most one is open, further ones are queued, identical ones are not repeated; `OK` closes. Connection dialogs close by themselves after a reconnect; output error dialogs stay.
- **A-39** Serial log at 115 200 baud: boot line with version, branch, commit and build time; I2C, SI5351 connection and errors, display detection, touch, settings. Verbose output (frequency changes, register values, bus scan) only with `LOG_DEBUG 1` (E-14).
- **A-40** `Wire` is never accessed while the bus is not open (`I2CBus::isOpen()`).
- **A-51** An internal error never freezes the device with running outputs: a failed LVGL assert (e.g. pool full) and a `loop()` blocked for 5 s (task watchdog) restart it; after the restart all outputs are off (A-4). Test hooks `TEST_LVGL_ASSERT` and `TEST_LOOP_HANG` (`config.h`) trigger both 20 s after boot.

### 3.7 Hardware detection

- **A-41** The SI5351 works with SDA/SCL in either order on CN1. The assignment is detected at boot and every 2 s while the chip is not ready (hot plug, replugging); while it is ready, the pins are never changed (E-17).
- **A-42** The identity of the device at 0x60 is checked before the first write; a foreign device is not programmed (at most a test byte in register 3).
- **A-43** An I2C line held low at boot (SI5351 reset during a read) is released.
- **A-44** One firmware for both boards: the display controller is detected before the display init; a boot gesture selects the other controller and stores the choice; a build flag can force it (E-18).

### 3.8 Build and tests

- **A-45** PlatformIO + Arduino-ESP32. `esp32-2432S028R` is the only firmware environment and `default_envs`; `native` (gcc in PATH) and `native-win` (bundled MinGW) run the host tests.
- **A-46** Pinned versions: `espressif32@6.7.0`, `bodmer/TFT_eSPI@2.5.43`, `lvgl/lvgl@8.3.11`, XPT2046_Touchscreen from GitHub, pinned to commit `d57f64c8b5f2bc5b8d10d121550806eeff7b06d9` (tag v1.4, E-4). Partition table `huge_app.csv`, 4 MB flash.
- **A-47** TFT_eSPI is configured only through build flags (`USER_SETUP_LOADED`), never by editing library files.
- **A-48** The host tests compile the hardware-independent sources (controller, SI5351 driver and planner, I2C bus, display detection, services, frequency utilities, logger) against fakes (E-15). These sources stay free of LVGL and TFT_eSPI; `constants.h`, `types.h` and `frequency_utils.h` do not include `Arduino.h`.
- **A-49** The version is `APP_VERSION` in `config.h`. Branch, commit (with `+` for uncommitted changes) and build time are taken at build time; without git they read `unknown` (E-24).
- **A-50** The own sources (`src/`) compile with `-Wall -Wextra` (`build_src_flags`) without warnings; libraries keep their default flags.

---

## 4. Decisions

### E-1 Fixed touch calibration
*Original specification.* Runtime calibration and its UI are disabled; all devices use `TOUCH_CAL_DEFAULT` (x 300–3800, y 300–3800). Touch works with these values on both boards (hardware tests I1, I2); hits at the screen edges are checked by B7.

### E-2 Global drive strength
*Original specification.* One CONFIG value `Drive all` for CH1–CH3; CH3 has no own setting, so master and follower of an IQ pair always drive alike. The per-channel fields were removed in 1.3.0 (stored key `drv_def` unchanged). Drive per channel is an open question (`todo.md` 9).

### E-3 RGB LED shows the open channel page
*Original specification, confirmed 2026-10-27.* The LED is not used as an output indicator.

### E-4 XPT2046 library from GitHub
*Original specification.* Installed by URL `#v1.4`, because the PlatformIO registry version was unreliable.

### E-5 Numpad with back key and validation
*Approved 2026-10-26 (1.1.0).* Original: no cancel button, 5-row grid. Now a 4 × 4 grid, back key `◀` to the channel page, current frequency as placeholder, invalid input rejected instead of clamped. Reason: keys were about 14 px high, and `MAIN` discarded the input and jumped one level too far.

### E-6 Correction in ppm
*Approved 2026-10-26 (1.1.0).* Original: buttons ±10 / ±1 without unit. Now shown in ppm, steps 0.01 / 0.1 / 1 ppm, hold-to-repeat, ±50 ppm. Reason: the unit is 0.01 ppm, so typical crystal errors needed hundreds of taps.

### E-7 IQ range 4.730–100 MHz, 180°/270° via CLK_INV
*Approved 2026-10-26 (1.1.0).* Original: 1–100 MHz with the phase register only. Reason: the former library clamped IQ at 1.4 MHz and ran the VCO below 600 MHz under 4.725 MHz (600 MHz / 127); with the phase register alone, 180°/270° were possible only from 14.3 / 21.4 MHz.

### E-8 CH3 ON as a request
*Approved 2026-10-26 (1.1.0).* Original: CH3 was disabled on an invalid phase. Now the request is kept (WAIT) and CH3 starts automatically. Reason: the user's choice was lost and CH3 never came back after a frequency change.

### E-9 Requested frequency stored exactly
*1.1.0, 2026-10-26.* The device keeps and shows what the user entered; the SI5351 rounding is shown separately as actual frequency (A-14).

### E-10 Own SI5351 register driver
*Approved 2026-10-27 (1.2.0).* Replaces osmanovv/esp32-si5351 plus direct writes into its internals. Reason: the library applied the correction to a frequency truncated to whole MHz, reset the PLL on every change and limited IQ (E-7). The own driver follows AN619 with exact correction, register cache, PLL reset only on change and supervision. Measured in host tests against the register model: ≤ 0.375 Hz for 1–100 MHz at correction 0. Formulas and register details are documented in `si5351_manager.*` and `si5351_plan.*`.

### E-11 All outputs start off
*Approved 2026-10-27 (1.2.0).* Before, stored ON/OFF states were restored. Reason: a connected mixer, amplifier or circuit under test must not get a signal before the user switches it on.

### E-12 Output error state ERR with silent retry
*1.2.0, 2026-10-27.* A setup that fails while the SI5351 answers is not a lost device: it is shown as ERR, the switch keeps the request, one dialog appears and the setup is retried every 2 s without further dialogs. Test hook `TEST_FAULT_IQ=1` (`config.h`) makes the IQ setup fail at 270°.

### E-13 ON/OFF text next to the header switch
*2026-10-27.* The text state stays next to the switch; it is readable without interpreting the switch graphic and also shows ERR.

### E-14 Verbose logging only with LOG_DEBUG
*1.2.0; bus scan 2026-10-28, frequency changes 1.3.0.* Blocking serial output slows down the GUI. `CORE_DEBUG_LEVEL=1`, because the I2C detection re-initializes the bus, which the core logs at info level.

### E-15 Host unit tests
*2026-10-27.* Driver math and controller logic are tested without hardware. `test/fakes` replaces Arduino, `Wire` (SI5351 register model with fault injection) and `Preferences`; `test/helpers/si5351_model.h` decodes the written registers independently of the driver's arithmetic, so a wrong formula cannot confirm itself. A check with deliberately inserted faults (correction sign, phase offset −1) made the affected tests fail.

### E-16 Frequency range 8 kHz–160 MHz (planned)
*Decided 2026-10-27, not implemented in 1.3.0* (`todo.md` 2.4). CH1/CH2 shall cover 8 kHz–160 MHz; CH3 stays at 4.730–100 MHz.

### E-17 Automatic SDA/SCL detection and identity check
*Requested 2026-10-28.* Reason: the original specification and the verified device disagreed on SDA/SCL, and cables get swapped in the lab.
- `AppController::connect_()` tries the open assignment first, then the other. Per assignment: address probe (0 ACK, 2 NACK, 5 timeout), then `SI5351Manager::begin()` with identity check. A failed init still tries the other assignment, because weak pull-ups can fake an ACK.
- Identity, read-only: register 0 = 0xFF, register 183 = 0x00 / 0xFF or XTAL_CL = 00 reject the device (`NotSi5351`). Register 183 bits 5:0 ≠ 010010b or a SYS_INIT that stays set only log a warning (clones). Then register 3 is written with 0xFF and read back; a mismatch rejects the device (`NoReadback`). Init writes stop at the first error.
- A rejected device is marked for its assignment. After `NotSi5351` the read-only check repeats every cycle (an SI5351 still powering up passes later); after `NoReadback` the device is left alone until it stops answering. A successful connection clears the mark.
- `LinkState`: NotFound (silent retry), Blocked (line held low), UnknownDevice, Connected. A dialog appears once per change to Blocked / UnknownDevice; at boot for every state except Connected.
- 100 kHz for detection, 400 kHz after init; the connection stays at 100 kHz if the read-only check fails at 400 kHz (weak pull-ups). `Wire` timeout 20 ms.
- Line release at boot: 9 clocks and a STOP on each pin (open drain with internal pull-up, 10 µs half period).
- Arduino-ESP32 2.0.x leaks the `Wire` lock on a transfer while the bus is not started, and the next transfer deadlocks (A-40). `I2CBus::open()` treats a failed `Wire.end()` as a closed bus.

### E-18 One firmware with display controller detection
*Requested 2026-10-28.* Replaces the separate environment `esp32-2432S028R-usbc`, because the boards look alike and the wrong firmware shows only noise. Measured on both boards (diagnostic sketch, 2026-10-28):

| | Original CYD | CYD2USB |
|---|---|---|
| MADCTL read-back (0Bh after writing A8h / 54h) | exact, no dummy bit | exact, no dummy bit |
| RDDID (04h, one dummy bit) | `00 00 00` | `81 81 B3` |
| RDID4 (D3h) without the 0xD9 index trick | not driven | not driven |
| MISO with internal pull-up / pull-down | data stays valid | the pull overrides the panel completely |
| Error-free read clock | up to 16 MHz | up to 8 MHz |
| Writes at 40 MHz, SPI mode 0 and 3 | error-free | error-free |

After the ILI9341 init sequence the ST7789 reports ID1 = 0x10 (C1h is IDSET there); SWRESET restores it.

- Probe before `tft.init()` at 1 MHz, mode 0, GPIO12 without pull: SWRESET; MADCTL A8h / 54h written and read back (the XOR must show FCh in bits 7:2, which rejects a floating or stuck MISO); RDDID read twice and compared. ID `000000` → ILI9341, `FFFFFF` → unknown, any other → ST7789 (unknown IDs logged). At most 3 attempts of about 20 ms.
- Order: build flag `DISPLAY_FORCE_PANEL` (1 / 2, nothing stored) → stored `disp_panel` → detection → ILI9341. The boot gesture (touch held 1.5 s from power-on, ≥ 90 % of 20 ms samples, plausible raw positions only) selects the other controller and stores it, or resets `disp_panel` to 0 when it equals the automatic result. A second gesture undoes a wrong choice without a readable screen.
- Init: TFT_eSPI (built for the ILI9341) sends its sequence on both boards; on the ST7789 `DisplayDetect::postInit()` sends SWRESET, 150 ms and `ST7789_INIT`, copied from TFT_eSPI 2.5.43 (hence the exact version pin), and `DisplayPanel` writes the ST7789 MADCTL (`08 68 C8 A8` for rotation 0..3 instead of `48 28 88 E8`).

### E-19 Tuning modes, default Fractional
*2026-10-28 (1.3.0).* Reason: in `Fractional` the MultiSynth almost always divides fractionally (host analysis with random frequencies of 1–100 MHz: 0 % in integer mode; with correction ≠ 0 not even 10, 25 or 50 MHz), which causes spurs near the carrier.
- `Low noise` follows the method of the QRP Labs and Etherkit VFOs: the largest even divider with VCO ≤ 900 MHz, kept with hysteresis while the VCO stays within 600–900 MHz (e.g. divider 90 covers 6.67–10 MHz). Whether the PLL takes small changes without reset is checked by measurements D5 / K6; if not, the PLL is reset on every change.
- `Integer` gives the lowest spurs and shows the fractional/integer difference most clearly in training. The raster is computed from the corrected crystal, so the correction cannot be compensated and only shifts the displayed actual frequency (7.1 MHz → 25 MHz × 25 / 88 = 7.102 273 MHz). `FBA_INT` / `FBB_INT` are set when the PLL ratio is integer and no spread spectrum runs on PLL A.
- Default stays `Fractional` (1.2.0 behavior, tuning without dropouts). Measurements K5 / K6 decide whether `Low noise` becomes the default.

### E-20 Spread spectrum on PLL A only, default Off
*2026-10-28 (1.3.0).* AN619 provides spread spectrum only on PLL A; the option is therefore called "Spread CH1". AN619 allows down spread −0.1 … −2.5 % and center spread ±0.1 … ±1.5 %; the selection is kept small (Off, −0.5 %, −1.5 %, ±0.5 %). Parameters (registers 149–161) are recomputed on every PLL A change. Spread spectrum needs a fractional PLL A, hence locked in `Integer`. The setting is written explicitly at init (`SSC_EN` = 0 for Off), because modules with programmed NVM could start with spread spectrum on. Default Off: spread spectrum widens the spectrum, which is unwanted in mixer experiments. The modulated VCO must stay within 600–900 MHz, so with spread the unmodulated VCO is limited to ≥ 600 MHz / (1 − amp) and, for center spread, ≤ 900 MHz / (1 + amp): `Fractional` uses the fixed PLLs 887.5 / 875 MHz for center spread, `Low noise` and IQ choose their divider in the narrower window. The IQ range with master CH1 therefore shrinks (−0.5 %: from 4.748 151 MHz; −1.5 %: from 4.796 355 MHz; ±0.5 %: 4.748 151–99.502 487 MHz); outside it CH3 waits instead of failing (`iqMinHz()` / `iqMaxHz()` in `si5351_plan.*`).

### E-21 Off state option, default Low
*2026-10-28 (1.3.0).* Up to 1.2.0 register 24 was never written; switched-off outputs were LOW (reset value 00), while the documentation wrongly claimed High-Z. `Low` stays the default: unchanged behavior and suitable for DC-coupled mixers (diode ring, FST3253). `High` drives DC into a DC-coupled load (current limited by the drive strength). Code 11 (never disable) is not offered.

Whether `CLKx_PDN` leaves a pin high-impedance or pulls it low is unmeasured (hardware test K2). Until then, outputs keep their driver powered with `High` and `High-Z` (output enable off only, also for CLK2); with `Low`, CLK2 is powered down as before. If K2 shows high impedance, `High-Z` will power down all switched-off outputs.

### E-22 CONFIG scrolls, all other pages stay fixed
*2026-10-28 (1.3.0).* Supersedes "no scrollbars" from 1.1.0, whose cause was unwanted scrollbars from overlapping elements. CONFIG needs seven rows of 48 px plus the info card (≈ 414 px) in 202 px below the header. Only the content area scrolls: snap to rows, no elastic overscroll, scrollbar always visible (4 px), the fifth row visibly cut at the bottom edge; the header with the save state stays fixed. The scroll position is kept in RAM when returning from MAIN. Scrolling starts after 10 px (`LV_INDEV_DEF_SCROLL_LIMIT`); a swipe that starts on a button gives `PRESS_LOST` and neither a click nor a repeat (hardware test K10).

### E-23 Backlight 10–100 % in 10 % steps
*2026-10-28 (1.3.0).* Replaces the slider and the 5 % minimum of 1.1.0. Reason: a swipe on a slider in the scrolling page would change the brightness; `−`/`+` without repeat need at most 9 taps. Older stored values are rounded to the nearest step; values below 5 % give 80 %. Readability at 10 % is checked by hardware test K12. PWM 5 kHz, 8 bit.

### E-24 Build information
*2026-10-28 (1.3.0).* `tools/build_info.py` (PlatformIO `extra_scripts = pre:`) determines branch, commit, dirty mark and local build time and passes them only to `src/utils/build_info.cpp` (`env.AddBuildMiddleware`), so each build recompiles just this file. `__DATE__` / `__TIME__` are unsuitable: the file would not be recompiled and the date would go stale. Shown on the CONFIG info card and in the boot log line; not part of the host tests.

### E-25 Documentation roles
*2026-10-28.* Specification = requirements and decisions; behavior only in `USER_MANUAL_DE.md`; `USER_MANUAL_EN.md` is its translation with the same structure and tables, updated in the same commit. Reason: the former specification (1401 lines) and both manuals described the same behavior and had drifted apart.
