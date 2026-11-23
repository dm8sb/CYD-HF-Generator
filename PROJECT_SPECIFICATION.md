# CYD HF Generator – Project Specification

This document contains only **requirements** and **decisions**, ordered by topic in nine chapters. Behavior as seen by the user (pages, controls, messages, log lines, examples) is described only in [USER_MANUAL_DE.md](USER_MANUAL_DE.md); [USER_MANUAL_EN.md](USER_MANUAL_EN.md) is its translation. Build instructions and the change history are in [README.md](README.md), architecture and development rules in [CLAUDE.md](CLAUDE.md), open findings and tasks in [ARBEITSPLAN.md](ARBEITSPLAN.md), measurements in [MESSPLAN.md](MESSPLAN.md).

**Numbering.** Requirements are `A-<chapter><nn>` (A-101, A-201 …), numbered and testable; decisions are `E-<chapter><nn>` (E-101, E-201 …) with rationale and measured values where they exist. Numbers are gap-free within a chapter and stable: a new requirement or decision gets the next free number of its chapter, a dropped one keeps its number with the note *dropped* or *superseded by*. This specification also holds the decisions formerly listed in the work plan and the former WEB-HMI concept (its security requirements are chapter 7). Origin notes such as *(step S55, audit SEC-5)* name work steps and audit findings from the git history.

| Chapter | Topic |
|---|---|
| 1 | Purpose, hardware and platform |
| 2 | Signal generation (SI5351) |
| 3 | Operation (GUI, display, touch) |
| 4 | Settings, persistence and diagnostics |
| 5 | Network and WiFi |
| 6 | Remote control (SCPI, LXI/VXI-11, REST, WebSocket) |
| 7 | Security |
| 8 | Build, tests, supply chain, release and CI |
| 9 | Documentation and licensing |

---

## 1. Purpose, hardware and platform

### 1.1 Purpose and use

A touchscreen-controlled 3-channel HF square-wave generator on the ESP32-2432S028R ("Cheap Yellow Display") with an SI5351 clock generator, optionally remote-controlled over USB and WiFi. It is used for experiments with HF mixers and for amateur radio training; low spurs and phase noise and an accurate IQ output matter more than feature count.

| Channel | SI5351 output | Function | PLL |
|---|---|---|---|
| CH1 | CLK0 | independent frequency | A |
| CH2 | CLK1 | independent frequency, or CH1 + offset (A-211) | B |
| CH3 | CLK2 | IQ follower of CH1 or CH2, phase 0/90/180/270° | PLL of its master |

Out of scope: OTA, external buttons, waveforms other than square wave (E-101).

### 1.2 Board variants

Two boards are sold as ESP32-2432S028R. They differ only in the display controller; pinout, touch, backlight and CN1 are identical. One firmware serves both (E-301).

| Board | Recognized by | Display controller | RDDID (04h), measured |
|---|---|---|---|
| Original CYD | micro-USB port only | ILI9341 | `00 00 00` |
| CYD2USB | additional USB-C port | ST7789 type | `81 81 B3` (datasheet ST7789V: `85 85 52`) |

### 1.3 Pin table

This table is authoritative; README and manual repeat parts of it.

| Function | Bus | GPIO | Defined in |
|---|---|---:|---|
| TFT MOSI | HSPI | 13 | build flags (`platformio.ini`) |
| TFT MISO | HSPI | 12 | build flags; no internal pull (E-301) |
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

### 1.4 SI5351 module

- SI5351A with 25 MHz crystal, I2C address 0x60, 3.3 V supply, connected to CN1.
- Crystal load capacitance 10 pF (register 183 = 0xD2).
- Many breakout boards have an active-low `OE` pin: OE = GND enables the outputs.
- The original specification listed SDA = 27 / SCL = 22; the verified wiring is SDA = 22 / SCL = 27. Both work (E-212).
- Clones (e.g. MS5351M) with a different register 183 are accepted with a log warning (E-212).
- Duty cycle and output level at 50 Ω are not yet measured (MESSPLAN M-309, M-312).

### 1.5 Requirements

- **A-101** GPIO21 is used only for the backlight PWM: never for I2C, buttons, LEDs or other peripherals, and never as an I2C candidate.
- **A-102** Display and touch use separate SPI buses (HSPI, VSPI).

### 1.6 Decisions

#### E-101 Feature scope
*(Decided by the user; the former "do not implement" list of the work plan.)* New functions after the release only as decided one by one; implemented so far: CH2 coupled to CH1 with an offset (E-213). Memory slots, sweep, drive strength per channel, an info page "Synthesizer", a free phase or a phase fine adjustment set by the user (only compensating a measured fixed skew of the IQ pair stays optional after the measurements), modulation, an external reference and a calibration assistant are not planned for now. Also not implemented: the RGB LED as an output indicator (it stays the page indicator, E-306); removing the double state display in the header (E-309); a channel selector in the header (rest of audit U-16: no room next to the state, MAIN and the cards reach every channel in 2 taps); a reduced RAM use display (former N13: the display suffices, RAM stays free); flash saving (former N15: with about 48 % used the flash has enough room); LVGL 9 (E-304); LovyanGFX except as a fallback (E-301); espressif32 6.13.0 (E-803); Secure Boot and flash encryption (E-701); OTA and NTP (OTA would need a new partition table; a later option), HTTPS (E-701).

---

## 2. Signal generation (SI5351)

### 2.1 Outputs and frequency

- **A-201** CH1 and CH2 generate independent square waves of 8 kHz–160 MHz (`FREQ_MIN_HZ`, `FREQ_MAX_HZ`, E-203), nominally 50 % duty cycle, settable with 1 Hz resolution.
- **A-202** The requested frequency is stored exactly, not the rounded SI5351 value (E-202). The numpad rejects values outside the range; `+`/`−` clamp to it.
- **A-203** Each output can be switched on and off independently. A switched-off output is disabled via the output enable register and shows the off state selected in CONFIG (A-208). The GUI shows the channel state as ON / OFF (plus ERR, WAIT, NO HW), never the off state.
- **A-204** All outputs are off after power-up; the ON/OFF state is not stored (E-205). After a reconnection of the SI5351 the previous states are restored.
- **A-205** One drive strength (2 / 4 / 6 / 8 mA) applies to all outputs (E-204).
- **A-206** Tuning mode `Fractional` / `Low noise` / `Integer`, global, default `Fractional` (E-206):

  | Mode | MultiSynth | PLL | Tuning |
  |---|---|---|---|
  | Fractional | fractional | fixed 900 or 887.5 MHz | no dropout; the PLL changes (with reset) only where 900 MHz cannot reach ≤ 0.25 Hz |
  | Low noise | even integer (`MS_INT`) | fractional, takes frequency and correction | PLL changes without reset while the divider stays (VCO 600–900 MHz); reset when the divider changes |
  | Integer | even integer | integer, 24–36 × crystal (`FBx_INT` for even ratios, AN619 3.2.1) | nearest raster point crystal × a / MS |

  A mode change reconfigures all outputs; one brief dropout of running outputs is allowed.
- **A-207** Spread spectrum `Off` / `-0.5 %` / `-1.5 %` / `+/-0.5 %` on PLL A only (CH1, and CH3 with master CH1), modulation ≈ 31.5 kHz, default `Off` (E-207). The setting is written explicitly at init (`SSC_EN` = 0 for `Off`). Not available in `Integer`; switching to `Integer` turns it off. Switching it resets PLL A (brief CH1 dropout, IQ realignment).
- **A-208** Off state `Low` / `High` / `High-Z` (register 24, `CLKx_DIS_STATE`), global, default `Low` (E-208). It is written at init right after the identity check, before any output can be enabled; a change applies at once and does not touch running outputs.
- **A-209** A MultiSynth that is not needed is powered down (CLK2 while CH3 is not in IQ mode), unless the off state needs the output driver (E-208).
- **A-210** A PLL is written and reset only when its parameters change. A reset affects only the PLL of the changed channel, never the channel on the other PLL. In `Fractional`, tuning a standalone output neither disables it nor resets its PLL, except for the rare PLL change of A-206.
- **A-211** CH2 can be coupled to CH1 (E-213): CH2 = CH1 + offset, offset −160…+160 MHz in 1 Hz steps (`CH2_OFFSET_MAX_HZ`), CH1 stays freely adjustable and every CH1 change retunes CH2 on its own PLL B through the normal path (A-210). Outside 8 kHz–160 MHz CH2 waits (CLK1 off, WAIT while switched on, also switching on keeps it off) and starts by itself when CH1 + offset fits again; a CH3 with master CH2 follows CH1 + offset (A-219 applies to it). Coupling and offset are stored (default free, 0; missing keys keep free); `Defaults` and `*RST` set free and 0. While coupled a CH2 frequency is refused (`Result::Coupled`, SCPI/REST −221); decoupling keeps the frequency CH2 last ran at.

### 2.2 Accuracy

- **A-212** Correction ±50 ppm in units of 0.01 ppm (`freq_correction`, positive = crystal runs fast). The crystal is modeled as 25 MHz × (1 + c × 10⁻⁸) and all dividers are computed against it with 64-bit integer math, so the correction is exact at every frequency. A change reconfigures the outputs without re-initializing the SI5351.
- **A-213** Maximum deviation of the synthesized from the requested frequency (register rounding only, correction assumed right):

  | Case | Limit | Host test |
  |---|---|---|
  | Standalone, `Fractional` | ≤ 0.5 Hz | ≤ 0.375 Hz at correction 0 |
  | Standalone, `Low noise` | ≤ 1.5 Hz | bound 25 MHz / 2^21 / 8 (divider 8 at 100 MHz); random frequencies max. 0.49 Hz |
  | IQ, `Fractional` / `Low noise` | ≤ 1.4 Hz | 1.32 Hz worst case (91 666 668 Hz) |
  | `Integer` | nearest raster point (kHz range possible) | – |
  | Standalone above 100 MHz (`Fractional`, `Low noise`) | ≤ 1.5 / 2 / 3 Hz up to 112.5 / 150 / 160 MHz | even integer MultiSynth 8 / 6 / 4 with a fractional PLL, bound 25 MHz / 2^21 / divider |

- **A-214** Fractions are the best rational approximation with a 20-bit denominator (continued fractions). VCO 600–900 MHz and the register limits of AN619 are kept. Accepted exception: in `Fractional` the fixed PLL of 36 × crystal exceeds 900 MHz by up to 45 kHz at +50 ppm.
- **A-215** The channel pages show the actual output frequency computed from the programmed registers (`actualHz()`), highlighted from 1 Hz deviation; with spread spectrum the mean frequency. MAIN shows the requested frequency, prefixed with `~` in `Integer` when the deviation is ≥ 1 Hz.

### 2.3 IQ (CH3)

- **A-216** CH3 has the frequency of its master (CH1 or CH2) and shares its PLL. All four phases 0/90/180/270° work over the whole IQ range 4.730–100 MHz (`IQ_MIN_HZ`, `IQ_MAX_HZ`) (E-209).
- **A-217** IQ pair: integer MultiSynth divider ≤ 127 (7-bit phase register; one unit = ¼ VCO period, so 90° = divider); 180°/270° = 0°/90° plus `CLK_INV` on CLK2. In `Fractional` and `Low noise` the divider is min(127, ⌊900 MHz / f⌋) with a fractional PLL; in `Integer` the PLL is integer as well.
- **A-218** Any change of PLL, divider or phase offset of an IQ pair disables both outputs, reconfigures them and resets the shared PLL to align the phases. A change of drive strength or inversion only is written without reset.
- **A-219** CH3 ON is a request (E-210): CLK2 runs only while the master is ON and inside the IQ range (4.730–100 MHz, narrower with spread spectrum on master CH1, see E-207). Otherwise CLK2 stays off without a dialog, the request is kept (WAIT) and CH3 starts automatically once the conditions are met.
- **A-220** If the IQ setup fails while the SI5351 answers, CLK2 is powered down, the master keeps running standalone, CH3 shows ERR and the setup is retried (A-228).

### 2.4 Connection, supervision and errors

- **A-221** The SI5351 works with SDA/SCL in either order on CN1. The assignment is detected at boot and on every search step while the chip is not ready (A-226) (hot plug, replugging); while it is ready, the pins are never changed (E-212).
- **A-222** The identity of the device at 0x60 is checked before the first write; a foreign device is not programmed (at most a test byte in register 3).
- **A-223** An I2C line held low at boot (SI5351 reset during a read) is released.
- **A-224** `Wire` is never accessed while the bus is not open (`I2CBus::isOpen()`).
- **A-225** SI5351 errors never crash the firmware; they are logged, shown as a dialog where useful, and leave the outputs in a safe state.
- **A-226** Supervision every 2 s (`HW_CHECK_INTERVAL_MS`) reads back the output enable and CLK control registers. A failed transfer or unexpected content (e.g. after a power interruption) marks the SI5351 as lost (NO HW); the firmware tries to reconnect in the same cycle and then every 2 s, after one minute without the chip every 10 s (each attempt reopens the I2C driver for both assignments, audit PERF-11), and restores all outputs with their states.
- **A-227** Without SI5351 no output is configured and no repeated errors appear; all settings remain editable and are saved.
- **A-228** An output setup that fails while the SI5351 answers is shown as ERR (MAIN, header, info line), the switch keeps the requested state, and the setup is retried every 2 s without further dialogs (E-211).

### 2.5 Decisions

#### E-201 Own SI5351 register driver
*Approved.* Replaces osmanovv/esp32-si5351 plus direct writes into its internals. Reason: the library applied the correction to a frequency truncated to whole MHz, reset the PLL on every change and limited IQ (E-209). The own driver follows AN619 with exact correction, register cache, PLL reset only on change and supervision. Measured in host tests against the register model: ≤ 0.375 Hz for 1–100 MHz at correction 0. Formulas and register details are documented in `si5351_manager.*` and `si5351_plan.*`.

#### E-202 Requested frequency stored exactly
*Decided.* The device keeps and shows what the user entered; the SI5351 rounding is shown separately as actual frequency (A-215).

#### E-203 Frequency range 8 kHz–160 MHz
*Decided and implemented.* CH1/CH2 cover 8 kHz–160 MHz; CH3 stays at 4.730–100 MHz (IQ divider 9…127) and waits above. Below 1 MHz the R divider (2^1…2^7) keeps the MultiSynth input at ≥ 1 MHz; the display switches to kHz. Above 100 MHz `Fractional` tries the largest even integer MultiSynth (8/6/4) with a fractional PLL when the fixed PLLs miss by more than 0.25 Hz; next to 112.5 MHz the MultiSynth ratio rounded to exactly 8 and missed by up to 5 Hz before (fixed in step S34). From 112.5 MHz on every step changes the PLL and resets it (short dropout). Level and harmonics are not measured yet (MESSPLAN M-111, M-112); the channel page shows a hint outside 1–100 MHz. On MAIN the `~` of `Integer` is left out from 100 MHz on (does not fit the card; the mode line shows `Integer`).

#### E-204 Global drive strength
*Original specification.* One CONFIG value `Drive all` for CH1–CH3; CH3 has no own setting, so master and follower of an IQ pair always drive alike. The per-channel fields were removed (stored key `drv_def` unchanged). Drive per channel is not planned (E-101).

#### E-205 All outputs start off
*Approved.* Before, stored ON/OFF states were restored. Reason: a connected mixer, amplifier or circuit under test must not get a signal before the user switches it on.

#### E-206 Tuning modes, default Fractional
*Decided.* Reason: in `Fractional` the MultiSynth almost always divides fractionally (host analysis with random frequencies of 1–100 MHz: 0 % in integer mode; with correction ≠ 0 not even 10, 25 or 50 MHz), which causes spurs near the carrier.
- `Low noise` follows the method of the QRP Labs and Etherkit VFOs: the largest even divider with VCO ≤ 900 MHz, kept with hysteresis while the VCO stays within 600–900 MHz (e.g. divider 90 covers 6.67–10 MHz). Whether the PLL takes small changes without reset is checked by measurements M-305 / M-303; if not, the PLL is reset on every change.
- `Integer` gives the lowest spurs and shows the fractional/integer difference most clearly in training. The raster is computed from the corrected crystal, so the correction cannot be compensated and only shifts the displayed actual frequency (7.1 MHz → 25 MHz × 25 / 88 = 7.102 273 MHz). `FBA_INT` / `FBB_INT` are set when the PLL ratio is integer and no spread spectrum runs on PLL A.
- Default stays `Fractional` (tuning without dropouts). Measurements M-301 / M-303 decide whether `Low noise` becomes the default (task FW-01).

#### E-207 Spread spectrum on PLL A only, default Off
*Decided.* AN619 provides spread spectrum only on PLL A; the option is therefore called "Spread CH1". AN619 allows down spread −0.1 … −2.5 % and center spread ±0.1 … ±1.5 %; the selection is kept small (Off, −0.5 %, −1.5 %, ±0.5 %). Parameters (registers 149–161) are recomputed on every PLL A change. Spread spectrum needs a fractional PLL A, hence locked in `Integer`. The setting is written explicitly at init (`SSC_EN` = 0 for Off), because modules with programmed NVM could start with spread spectrum on. Default Off: spread spectrum widens the spectrum, which is unwanted in mixer experiments. The modulated VCO must stay within 600–900 MHz, so with spread the unmodulated VCO is limited to ≥ 600 MHz / (1 − amp) and, for center spread, ≤ 900 MHz / (1 + amp): `Fractional` uses the fixed PLLs 887.5 / 875 MHz for center spread, `Low noise` and IQ choose their divider in the narrower window. The IQ range with master CH1 therefore shrinks (−0.5 %: from 4.748 151 MHz; −1.5 %: from 4.796 355 MHz; ±0.5 %: 4.748 151–99.502 487 MHz); outside it CH3 waits instead of failing (`iqMinHz()` / `iqMaxHz()` in `si5351_plan.*`).

#### E-208 Off state option, default Low
*Decided.* Originally register 24 was never written; switched-off outputs were LOW (reset value 00), while the documentation wrongly claimed High-Z. `Low` stays the default: unchanged behavior and suitable for DC-coupled mixers (diode ring, FST3253). `High` drives DC into a DC-coupled load (current limited by the drive strength). Code 11 (never disable) is not offered.

Whether `CLKx_PDN` leaves a pin high-impedance or pulls it low is unmeasured (measurement M-118). Until then, outputs keep their driver powered with `High` and `High-Z` (output enable off only, also for CLK2); with `Low`, CLK2 is powered down as before. If M-118 shows high impedance, `High-Z` will power down all switched-off outputs (task FW-05).

#### E-209 IQ range 4.730–100 MHz, 180°/270° via CLK_INV
*Approved.* Original: 1–100 MHz with the phase register only. Reason: the former library clamped IQ at 1.4 MHz and ran the VCO below 600 MHz under 4.725 MHz (600 MHz / 127); with the phase register alone, 180°/270° were possible only from 14.3 / 21.4 MHz.

#### E-210 CH3 ON as a request
*Approved.* Original: CH3 was disabled on an invalid phase. Now the request is kept (WAIT) and CH3 starts automatically. Reason: the user's choice was lost and CH3 never came back after a frequency change.

#### E-211 Output error state ERR with silent retry
*Decided.* A setup that fails while the SI5351 answers is not a lost device: it is shown as ERR, the switch keeps the request, one dialog appears and the setup is retried every 2 s without further dialogs. Test hook `TEST_FAULT_IQ=1` (`config.h`) makes the IQ setup fail at 270°.

#### E-212 Automatic SDA/SCL detection and identity check
*Requested.* Reason: the original specification and the verified device disagreed on SDA/SCL, and cables get swapped in the lab.
- `AppController::connect_()` tries the open assignment first, then the other. Per assignment: address probe (0 ACK, 2 NACK, 5 timeout), then `SI5351Manager::begin()` with identity check. A failed init still tries the other assignment, because weak pull-ups can fake an ACK.
- Identity, read-only: register 0 = 0xFF, register 183 = 0x00 / 0xFF or XTAL_CL = 00 reject the device (`NotSi5351`). Register 183 bits 5:0 ≠ 010010b or a SYS_INIT that stays set only log a warning (clones). Then register 3 is written with 0xFF and read back; a mismatch rejects the device (`NoReadback`). Init writes stop at the first error.
- A rejected device is marked for its assignment. After `NotSi5351` the read-only check repeats every cycle (an SI5351 still powering up passes later); after `NoReadback` the device is left alone until it stops answering. A successful connection clears the mark.
- `LinkState`: NotFound (silent retry), Blocked (line held low), UnknownDevice, Connected. A dialog appears once per change to Blocked / UnknownDevice; at boot for every state except Connected.
- 100 kHz for detection, 400 kHz after init; the connection stays at 100 kHz if the read-only check fails at 400 kHz (weak pull-ups). `Wire` timeout 20 ms.
- Line release at boot: 9 clocks and a STOP on each pin (open drain with internal pull-up, 10 µs half period).
- Arduino-ESP32 2.0.x leaks the `Wire` lock on a transfer while the bus is not started, and the next transfer deadlocks (A-224). `I2CBus::open()` treats a failed `Wire.end()` as a closed bus.

#### E-213 CH2 coupled to CH1 with an offset
*(Step N14; decided by the user.)* A superheterodyne test setup needs LO and test signal at a fixed distance (IF); retuning both by hand doubles every step. CH2 therefore follows CH1 + offset when coupled. Operation as decided by the user: chips `Free` / `CH1 + offset` on the CH2 page, the frequency card shows the offset, `+`/`−` and the numpad change it, the MAIN mode line shows e.g. `CH1 +455 kHz`; offset −160 … +160 MHz in 1 Hz; outside 8 kHz–160 MHz CH2 waits (`WAIT`) and starts again by itself. Decisions: the coupling lives in `AppController` (single source of truth) as `Ch2Coupling {coupled, offset}` in `PersistentState`, NVS keys `ch2_cpl` (U8) and `ch2_off` (I32), separate from `ch2_freq`, so old NVS contents stay free. While coupled and in range `ch2.frequency` is kept at CH1 + offset (one more key in the same autosave), so decoupling keeps the running frequency without a jump and every reader of the stored state sees a valid frequency; `frequency(2)` gives the target also while waiting (0 below 0 Hz). Waiting works like CH3 below the IQ range: CLK1 is powered down (A-209) instead of an error, so the user can move CH1 through a band edge without dialogs; switching CH2 on meanwhile keeps register 3 closed, because with off state High/High-Z the driver stays powered with the old MultiSynth setting. CH2 keeps PLL B (no new PLL sharing, no spread spectrum); a CH1 change retunes CH2 in a second, independent setup, without dropout in `Fractional` (host test: no PLL reset, register 3 untouched). An offset range of ±160 MHz (not limited by the current CH1) lets an offset be entered before CH1 moves; setters never clamp it, `adjustCh2Offset()` does. A CH2 frequency while coupled gets its own `Result::Coupled` (SCPI/REST −221 `CH2 is coupled to CH1`) instead of `Conflict`, whose text is the Integer/spread conflict, so the web server needs no change. REST orders offset before coupling (no jump to CH1 + old offset) and coupling before the frequency (decouple, then set). The CH2 page gets the chips as an extra row below the step chips (the step chips stay right under the frequency card, user test; rows 32–36 px instead of 36/62 px), CH1 keeps its layout. Flash +4 196 B (1 531 270 B), static RAM +128 B (78 736 B).

---

## 3. Operation (GUI, display, touch)

### 3.1 Display and board detection

- **A-301** One firmware for both boards: the display controller is detected before the display init at every boot; nothing is stored; a build flag can force it (E-301, E-302).
- **A-302** The backlight stays off until the first frame is drawn (the uninitialized panel shows noise).
- **A-303** The screen can be turned by 180° (E-314) for mounting with the sockets on the other side: CONFIG `Device` → `Screen` → `Rotate` toggles 0° (default) and 180°, applied at once without a restart on both display controllers, with the touch following the rotated controls and the stored touch calibration valid in both orientations (the calibration wizard works in both). The orientation is stored (missing key: 0°); `Defaults` and `Factory` set 0°, `*RST` sets 0° in RAM like the backlight. Remote: SCPI `SYSTem:DISPlay:ROTation 0|180`, REST `config.rotation`.

### 3.2 Pages and layout

- **A-304** LVGL 8 (8.4.0, E-304), 320 × 240 landscape, dark theme with colored cards. Pages: MAIN, CH1, CH2, CH3, CONFIG, NUMPAD, the touch calibration and the WiFi pages (WIFI with network scan and text entry, access point, device).
- **A-305** Page coordinates derive from the layout grid in `gui/gui_layout.h` (margin 8 px, header 38 px, card padding 6 px). No control is clipped or overlaps another. Pages do not scroll (E-311). Every page except MAIN has the same header: back button `◀` at the top left (one level up; CH1–CH3 and CONFIG go to MAIN), the title after it, page actions at the right; lists page with `▲`/`▼` (E-312).
- **A-306** Frequently used controls are at least about 36 px high; yellow buttons use dark text.
- **A-307** Colors: CH1 red, CH2 green, CH3 blue, CONFIG yellow, MAIN and back buttons gray. The RGB LED shows the color of the open channel page and is off on all other pages (E-306).
- **A-308** Frequencies are shown in MHz with grouped digits (`10.700 000 MHz`).
- **A-309** MAIN has no title header; it shows one tappable card per channel with state, frequency and its own switch, and at the bottom a status bar with save errors and the `CONFIG` button (the cards open the channel pages; the former CONFIG row `Step` is gone, its chips are on the channel pages). State and detail lines of the cards use 14 px, states other than OFF as filled labels (audit U-8). The channel pages (CH1–CH3) and CONFIG have a header with title and `MAIN` button; channel pages add the ON/OFF switch (request) with the actual state as on MAIN, `ON` / `OFF` / `WAIT` / `! ERR` / `! NO HW` (E-309). The card switches sit in their own column, at least 12 px from other touch targets (audit U-3).
- **A-310** Settings that change the signal compared to the defaults (off state, spread spectrum, tuning mode, drive strength) are visible on MAIN and on the channel pages, not only in CONFIG; off state `High` is shown in the warning color and needs a confirmation before it applies at the touch screen and in the web UI (audit U-2); remote sources (SCPI over USB, TCP and VXI-11, REST) may select it only with `Remote outputs: allowed`, under `local only` it is refused like switching an output on (SCPI `-221`, REST 403; E-611).
- **A-311** Touch targets: dialog buttons at least 120 × 40 px with at least 3:1 contrast to the dialog (audit U-4); buttons at most 8 px from the screen edge take taps up to the edge, without growing towards their neighbors (U-5); the header `MAIN` button is 36 px high; the CH3 master and phase chips are 6 px apart (U-6).
- **A-312** Contrast (WCAG 2.x, checked by `test_contrast`): text on colored buttons and chips in the color with the higher contrast (dark on CH1/CH2/CH3, at least 4.2:1), switch track and chip border at least 3:1 against the background (audit U-7). States other than OFF are filled labels, errors start with `!`, so they differ also without color (U-19).
- **A-313** The CH3 page sets the master frequency (tap on the frequency, numpad, back to CH3) and switches the master on while CH3 waits for it (audit U-16).
- **A-314** CONFIG contains, signal settings first, drive strength, off state, tuning mode, spread spectrum, correction (E-308), backlight (10–100 % in 10 % steps, E-310) and an info card (firmware version, branch, commit with dirty mark, build time, display controller with its source, SI5351 pins or state). Save errors are visible in every tab; the normal states `Saved` / `Saving...` are not shown on the device (E-313). CONFIG is divided into five tabs with at most three rows each, chosen on a tab bar at the bottom (E-311).
- **A-315** Dialogs: at most one is open, further ones are queued, identical ones are not repeated; `OK` closes. Connection dialogs close by themselves after a reconnect; output error dialogs stay. The same holds for confirmation dialogs (two buttons), also for automatic questions (open fallback AP, web network change): an answer acts only on the dialog that showed it (generation per box, button by index), a question that is no longer needed is closed without effect, and the open fallback AP is allowed only while it waits for its question (audit CQ-1, CQ-15).

### 3.3 Input

- **A-316** Frequency entry for CH1/CH2: step presets 1 Hz … 1 MHz, `+`/`−` with hold-to-repeat (from 0.6 s about 3 steps/s, after 3 s 10/s, after 6 s 20/s; pressure dropouts up to 70 ms keep the hold, audit U-20), tap on a digit selects the step (a separator selects the nearer neighbor, audit U-15), numeric keypad with validation (E-307) that rounds to 1 Hz exactly like remote input (half up, in integers, one shared routine for the numpad, REST and SCPI); a rejected input stays editable with the reason shown, the current frequency stays visible (U-9).
- **A-317** The frequency correction can also be typed on the numpad (tap the value in CONFIG): signed, rounded to 0.01 ppm, at most ±50.00 ppm; a value outside is rejected and stays editable (audit U-11).
- **A-318** Lab operation (audit U-13): a screen lock (CONFIG, unlock by holding 2 s anywhere, the unlocking touch triggers nothing, not stored), a reset to defaults that keeps the frequency correction and the touch calibration and switches all outputs off, and a factory reset that erases the NVS and restarts; both resets need a confirmation.
- **A-319** Touch: XPT2046 with a calibration per board (wizard with four corner crosses and a check cross, stored in NVS; fixed default calibration without one, E-303), jitter filter and debounce.

### 3.4 Decisions

#### E-301 One firmware with display controller detection
*Requested.* Replaces the separate environment `esp32-2432S028R-usbc`, because the boards look alike and the wrong firmware shows only noise. Measured on both boards (diagnostic sketch, measurement M-526):

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
- Order: build flag `DISPLAY_FORCE_PANEL` (1 / 2) → detection → ILI9341. A boot gesture that could select the other controller and store it in `disp_panel` was removed (E-302).
- Init: TFT_eSPI (built for the ILI9341) sends its sequence on both boards; on the ST7789 `DisplayDetect::postInit()` sends SWRESET, 150 ms and `ST7789_INIT`, copied from TFT_eSPI 2.5.43 (hence the exact version pin), and `DisplayPanel` writes the ST7789 MADCTL (`08 68 C8 A8` for rotation 0..3 instead of `48 28 88 E8`).
- TFT_eSPI 2.5.43 (its last release) stays; it ran unchanged on core 3.x (flush 35 ms per full frame as under 2.x). A fork or LovyanGFX only as a way out; the own detection stays, because LovyanGFX expects the ST7789 ID 0x85 while the CYD2USB reports `8181B3`.

#### E-302 Boot gesture removed, detection only
*Decided.* The boot gesture (touch held 1.5 s from power-on: use the other controller and store it) could fire by accident with a thumb on the screen while plugging in, and stored a wrong controller that only a second gesture without a readable screen undid (audit U-10). The detection (MADCTL read-back, RDDID read twice, up to 3 attempts) worked reliably on both boards in all tests. Decision: no runtime override; the build flag `DISPLAY_FORCE_PANEL` stays as the fallback for an unknown board. The NVS key `disp_panel` would otherwise override the detection forever and is deleted at boot. A touch at power-on is still kept away from LVGL.

#### E-303 Touch calibration per board
*Decided (supersedes the fixed calibration of the original specification).* Originally runtime calibration and its UI were disabled and all devices used `TOUCH_CAL_DEFAULT` (x 300–3800, y 300–3800); touch worked with these values on both boards (measurements M-527, M-528). The hit map (MESSPLAN M-126, test build of step S28) then showed this fixed calibration off on both boards: hits 11–19 px too far left on the original CYD, x = 80 by about −17 px and the right edge by −1 … −7 px on the CYD2USB; the right corner was not reachable on the original CYD (audit U-26). Requested by the user: a calibration at the very first start. Four crosses 20 px from the corners give offset and slope per axis (linear, as `mapPoint()`), the cross in the middle checks it (≤ 8 px; the hit map scattered by about ±5 px with a good calibration); each cross is held 0.5 s, the first 0.1 s are not averaged. First results: x 161…3778 / y 245…3830 (CYD2USB) and x 166…3716 / y 280…3885 (original CYD), check −2/+2 and −2/+1 px; the original CYD after a factory reset x 158…3708 / y 245…3856, check −2/+1 px. Stored in NVS outside the autosave; without a stored calibration the wizard runs at boot, 30 s without a touch continue with the default, so a board with a defective touch stays usable. A CONFIG row starts it again.

#### E-304 LVGL 8.4.0, not LVGL 9
*(Migration step S40a.)* 8.4.0 is the last LVGL 8 release. Against 8.3.11 it changes almost only formatting; in content the screen load order, a group focus swap and a cast in `lv_obj_style.h` that removes the C++20 enum warning of GCC 14 (Arduino 3.x); the widgets the GUI uses (label, button matrix, dropdown, switch, textarea, message box, scrolling, input) are unchanged apart from whitespace, `lv_conf_internal.h` and the template keys are identical. Measured on the original CYD (debug build): first frame 159 ms (2.x core, as 8.3.11) and 169 ms (3.x core, 172 ms with 8.3.11), heap and LVGL memory unchanged, flash +72 B. LVGL 9 is not used: new display, input, style and widget APIs would mean rebuilding every page and a full operating test, for no feature the device needs.

#### E-305 LVGL memory pool from the heap
*(Step S53b.)* With the WiFi stack the static data reached 124 KB and the next 1 KB (scan list) overflowed the DRAM segment `dram0_0_seg` by 728 bytes. The 48 KB LVGL pool is therefore allocated with `malloc` in `lv_init()` (`LV_MEM_POOL_ALLOC`) instead of being a static array: static RAM 76 KB, after boot with WiFi in Client mode 136 KB heap free, minimum 129 KB, largest block 61 KB (warning limit 40 KB). The pool size is unchanged; the allocation happens once at boot, before any network buffer.

#### E-306 RGB LED shows the open channel page
*Original specification, confirmed.* The LED is not used as an output indicator.

#### E-307 Numpad with back key and validation
*Approved.* Original: no cancel button, 5-row grid. Now a 4 × 4 grid, back key `◀` to the channel page, current frequency as placeholder, invalid input rejected instead of clamped. Reason: keys were about 14 px high, and `MAIN` discarded the input and jumped one level too far.

#### E-308 Correction in ppm
*Approved.* Original: buttons ±10 / ±1 without unit. Now shown in ppm, steps 0.01 / 0.1 / 1 ppm, hold-to-repeat, ±50 ppm. Reason: the unit is 0.01 ppm, so typical crystal errors needed hundreds of taps.

#### E-309 ON/OFF text next to the header switch
*Changed (audit U-1).* The text state stays next to the switch; it is readable without interpreting the switch graphic. It shows the actual state like MAIN (also WAIT and NO HW) instead of the request, which the switch already shows: before, the header said ON while CH3 was waiting or the SI5351 was missing.

#### E-310 Backlight 10–100 % in 10 % steps
*Decided.* Replaces the slider and its 5 % minimum. Reason: a swipe on a slider in the then scrolling CONFIG page would change the brightness; `−`/`+` without repeat need at most 9 taps. Older stored values are rounded to the nearest step; values below 5 % give 80 %. Readability at 10 % is checked by measurement M-436. PWM 5 kHz, 8 bit.

#### E-311 CONFIG in tabs instead of a scrolled list
*(User tests; supersedes the scrolled CONFIG page.)* Before, CONFIG scrolled while all other pages stayed fixed: seven, later six rows of 50 px (the `Step` row duplicated the chips of the channel pages, audit U-14) in the order Drive, Off state, Tuning, Spread, Corr., Backlight (signal settings first), buttons 40 px (U-18), snapping to rows, scrollbar always visible (6 px, 5.7:1, U-24), position kept in RAM; scrolling started after 10 px (`LV_INDEV_DEF_SCROLL_LIMIT`), a swipe that started on a button gave `PRESS_LOST` and neither a click nor a repeat (measurement M-435); a "more" label at the bottom edge was tried and dropped in the operating test. On the resistive touch panel scrolling the CONFIG list worked poorly and did not match the rest of the GUI, where lists page (WIFI scan list); in the user tests paging between several pages was rated better. CONFIG therefore has five tabs on a tab bar at the bottom (36 px, five tabs of 60 px): `Signal` (Drive all, Off state, Tuning), `Freq` (Spread CH1, Corr.), `Net` (WiFi with Setup, Remote out., Clients with `Reconnect all`), `Device` (Backlight, Screen with Lock and touch calibration, Reset) and `Info` (info card). Each tab shows at most three rows of 50 px, so rows, buttons (40 px) and dropdowns keep their size and nothing scrolls; dropdown lists use 9 px line spacing, so a 4-entry list (103 px) opens completely. Tabs instead of `◀ ▶` paging, because every tab is one tap away. `Reconnect all` moved from the info card to `Net` and the touch calibration next to the screen lock, because the info card (8 lines) and the reset row do not fit one tab together. The selected tab is kept in RAM until power-off; a tab change rebuilds the page like any navigation.

#### E-312 One back button for every page
*(User tests, together with E-311.)* The channel pages and CONFIG had a `MAIN` button at the top right, the WIFI pages and the numpad a back button `◀` at the top left in two widths (48 and 52 px), and the scan list paged with the same `<` `>` symbols. Now every page except MAIN has the same header: `◀` at the top left (52 × 36 px, up to the edges, U-5) goes one level up, the title follows, page actions (`Device`, `Scan`) sit at the right at the same height. For CH1–CH3 and CONFIG one level up is MAIN, so the `MAIN` button is gone; from the WIFI pages `◀` returns to CONFIG (tab kept) or the WIFI overview, from the numpad without a change to the page before. The scan list pages with `▲`/`▼`, so `◀` always means back.

#### E-313 No `Saved` / `Saving...` on the device
*(User tests.)* The save indicator in the CONFIG header and the MAIN status bar irritated users: `Saving...` appeared after every tap and changed to `Saved` a few seconds later without anything to do. The device now shows only the error states in red (`Save failed`, `Not saved`), which need an action; the normal states stay empty. The manual says that changes are saved a few seconds after the last one. The web interface keeps its save state (it has a `Save` button that saves at once).

#### E-314 Screen rotation by 180° through MADCTL, touch mirrored after the calibration
*(Step N16, decided by the user: `Rotate` as the third button of the row `Screen` in the tab `Device`, next to `Lock` and `Touch cal.`.)* In a case the USB and SMA sockets may have to point the other way. Decisions: the rotation is a `SystemSettings` field (`rotation`, degrees 0/180) in `AppController`, NVS key `rot180` (U8, 1 = 180°; missing or other values: 0°), so it follows the backlight: remote setters through the command queue, `Defaults` saves 0°, `*RST` sets 0° in RAM without saving. Keeping it across `*RST` like the touch calibration was considered; `*RST` already resets the other display setting (backlight) and a restart restores the stored orientation, so it follows the backlight (E-608: `*RST` = defaults in RAM). The GUI applies a changed value from any source on its next update (`DisplayPanel::setTurned()`, `TouchDriver::setTurned()`, full invalidation, the calibration restarts in the new orientation); the controller stays free of display code. Display: landscape rotation 1 upright, 3 turned. The MADCTL values differ between 1 and 3 in MX and MY only (ILI9341 via TFT_eSPI 0x28 → 0xE8, ST7789 table 0x68 → 0xA8): reversing both the column and the row address order is a point reflection about the center, i.e. a 180° rotation and no mirror image, while MV (row/column exchange) and BGR stay; this matches TFT_eSPI 2.5.43 `ILI9341_Rotation.h` and `ST7789_Rotation.h` rotation 3. Both panels use their full 240 × 320 RAM, so no window offset changes (TFT_eSPI's `colstart = 80` for rotation 3 applies only to 240 × 240 ST7789 panels). `test_display_detect` pins the XOR relation for both tables. Touch: the XPT2046 stays in its landscape setting and the calibration stays in raw coordinates of the upright screen; the position is mirrored after the calibration (x' = 319 − x, y' = 239 − y, `TouchFilter::rotate180`), so one calibration serves both orientations. The wizard draws its crosses in screen coordinates; turned, the cross at corner i lies on the panel's corner (i + 2) mod 4, so it reorders the corners before computing (the targets are symmetric to the center) and judges the check cross after mirroring; `test_touch_cal` covers mapping and wizard in both orientations. The CONFIG row `Screen` takes `Rotate` as a third button (Lock 60, Touch cal. 100, Rotate 70 px, from x 56, 40 px high). Flash +1 088 B (1 532 358 B), static RAM +8 B (78 744 B).

#### E-315 No further accessibility work
*(Decided by the user.)* Priority 5 of the project audit (accessibility, AF40–AF43) is not implemented: device and web UI are usable well enough for the use case. With it the planned single-key shortcuts of the web UI off by default with a switch in the help dialog, the CH1 text color `#F4615D` on the device and Montserrat 14 in the grey info and mode lines are dropped; the A11Y findings stay only as names.

---

## 4. Settings, persistence and diagnostics

### 4.1 Settings and persistence

- **A-401** Settings are stored in Preferences/NVS, namespace `hfgen`. Every value is validated on load:

  | Key | Type | Content | Default | On load |
  |---|---|---|---|---|
  | `ch1_freq`, `ch2_freq` | U64 | Hz | 10 MHz, 14 MHz | clamped to 8 kHz–160 MHz |
  | `ch3_master` | U8 | 1 / 2 | 1 | invalid → default |
  | `ch3_phase` | U32 | 0 / 90 / 180 / 270 | 90 | invalid → default |
  | `backlight` | U8 | % | 80 | < 5 or > 100 → 80, otherwise nearest 10 % step |
  | `delta` | U32 | step in Hz | 1000 | nearest preset 1 Hz … 1 MHz |
  | `corr` | I32 | 0.01 ppm | 0 | clamped to ±5000 |
  | `drv_def` | U8 | mA | 4 | invalid → default |
  | `tune` | U8 | 0 Fractional, 1 Low noise, 2 Integer | 0 | invalid → default |
  | `ssc` | U8 | 0 Off, 1 −0.5 %, 2 −1.5 %, 3 ±0.5 % | 0 | invalid → default; Off in Integer |
  | `off_st` | U8 | 0 Low, 1 High, 2 High-Z | 0 | invalid → default |
  | `tc_x0`, `tc_x1`, `tc_y0`, `tc_y1` | I32 | raw touch values of the screen edges (x 0 / 319, y 0 / 239) | none | written by the calibration wizard; missing or implausible → default and wizard (E-303) |
  | `disp_panel` | U8 | removed | – | was written by the former boot gesture; deleted at boot (E-302) |

- **A-402** Missing keys give the defaults, so older NVS contents keep their behavior. The former keys `ch1_en` / `ch2_en` / `ch3_en` are ignored. Output states and the SDA/SCL assignment are not stored; the touch calibration is stored by its wizard, outside the autosave (E-303).
- **A-403** Autosave writes all settings 5 s after the last change (`SETTINGS_AUTOSAVE_MS`).
- **A-404** The autosave writes only the NVS keys that changed; changes from SCPI (USB, TCP, VXI-11) are saved after 30 s of quiet, touch and web (REST) changes after 5 s; `Remote out.`, the network settings and the web password are saved at once outside the autosave; `*RST` sets the defaults in RAM without saving them until `SYSTem:SAVE`, and a later change saves only its own key (V-4, C-17, E-402, E-608). `SYSTem:SAVE` and `POST /api/save` write only the keys that differ from NVS (all after `*RST`), at most once per 10 s per source; a further request within 10 s is merged without an error and saved 10 s after the previous save (AF17, E-402).
- **A-405** Without NVS the firmware runs with defaults and saves nothing. NVS unavailable at boot and a failed save each produce a dialog and a save state on CONFIG. A failed save keeps the change and is retried every 30 s (`SETTINGS_RETRY_MS`); the dialog appears once per series of failures.

### 4.2 Log and watchdog

- **A-406** Serial log at 115 200 baud: boot line with version, branch, commit and build time; I2C, SI5351 connection and errors, display detection, touch, settings. Verbose output (frequency changes, register values, bus scan) only with `LOG_DEBUG 1` (E-401).
- **A-407** The log output (own lines, LVGL and ESP-IDF log) is switchable between the serial port, a ring buffer of 32 lines and off, so a SCPI session over USB gets no log lines (V-5, C-16, E-612). Control characters inside a line are written as `?`, SSIDs go through `Sanitize::text` (audit SEC-6). `SYSTem:LOG?` reads the buffer only for the USB source; over TCP and VXI-11 it answers `""`.
- **A-408** An internal error never freezes the device with running outputs: a failed LVGL assert (e.g. pool full) and a `loop()` blocked for 5 s (task watchdog) restart it; after the restart all outputs are off (A-204). Test hooks `TEST_LVGL_ASSERT` and `TEST_LOOP_HANG` (`config.h`) trigger both 20 s after boot.

### 4.3 Decisions

#### E-401 Verbose logging only with LOG_DEBUG
*Decided.* Blocking serial output slows down the GUI. `CORE_DEBUG_LEVEL=1`, because the I2C detection re-initializes the bus, which the core logs at info level.

#### E-402 Saving per source
*(WEB-HMI concept; audit SEC-V-3, AF17, decided by the user.)* Touch and web changes are saved 5 s after the last change, SCPI changes after 30 s of quiet (a script sends many commands, each save costs flash wear); only changed keys are written; switching an output on or off is never saved (A-204). `SYSTem:SAVE` and `POST /api/save` save at once, but at most once per 10 s per source; further requests are merged silently (saved at the latest 10 s after the previous save) without an error, and the explicit save writes only the changed values.

---

## 5. Network and WiFi

WiFi client or own access point (default off), a web interface with all functions of the touch screen, SCPI over USB and TCP 5025, LXI/VXI-11 and REST. No sniffing of WiFi passwords, no remote control through foreign web pages, no unintended transmission; operation at the device has priority. Behavior: manual sections 8 (WIFI pages, info card), 15 (SCPI, LXI/VXI-11) and 16 (web interface, REST); module rules in [CLAUDE.md](CLAUDE.md).

### 5.1 Requirements

- **A-501** WiFi operation (E-501, A-702, A-703): `Off` keeps the radio off. `Client` scans asynchronously and connects to the strongest stored network (a network not seen, e.g. hidden, is tried in turn), at least WPA2 unless the network was stored as open, 15 s per attempt; without an address for 60 s it starts the fallback AP and keeps searching. The AP (`Access point` mode or fallback) is `CYD-HF-<last 4 hex digits of the MAC>` at 192.168.4.1/24 with WPA2 and the random password (created on first use while the radio runs), at most 4 stations (open: 2, and only after a touch confirmation as fallback; off after 15 min without station); its DHCP server offers no router but itself as DNS server, a captive DNS answers every name with 192.168.4.1. As soon as the client has an address the AP goes off within 1 s (E-511). mDNS announces `<name>.local`. The WiFi driver stores nothing itself (`WiFi.persistent(false)`). Nothing of it runs in the WiFi task; `loop()` is never blocked (no `WiFiMulti::run()`).
- **A-502** Network settings live in their own NVS namespace `hfnet`: mode (default Off), at most two networks, AP password, open AP flag, device name, static IP. A WiFi key is stored as entered: passphrase (8–63 characters) or 64-digit hex PMK (E-510), accepted the same way at the touch keyboard, over REST and in the web UI (`NetConfig::validNetworkKey`); it is never shown, logged or exported; the AP password is 12 random characters without 0/O/1/l/I; the device name follows RFC 1123 (default `cyd-hf-<last 4 hex digits of the MAC>`); a static IP must pass the plausibility check (contiguous mask /1–/30, address and gateway different and in one subnet, neither network nor broadcast address), else DHCP applies. Invalid stored values fall back to the defaults. The factory reset erases `hfnet` and the WiFi driver's namespace `nvs.net80211` (E-503, E-505, E-507, A-712, A-714, A-716).
- **A-503** CONFIG has the rows `WiFi` (Off / Client / Access point, saved and applied at once) and `Remote out.` (allowed / local only, A-612). The MAIN status bar shows the WiFi state (symbol green connected, gray searching; `AP` green, `AP open` in the warning color) and for 10 s after a remote command `REMOTE <source>`, with `LOCKED` taking precedence (shape, not only color, audit U-19); no element overlaps another in any combination (widths from the font data, at least 8 px before `CONFIG`). The info card shows the WiFi state and the heap (free, minimum, largest block, LVGL). A foreign string (SSID) reaches LVGL only through `Sanitize::text` (valid UTF-8, no control characters, at most 32 bytes) (A-709, V-8).
- **A-504** WIFI page (`Setup` in the CONFIG row `WiFi`, no scrolling): two network slots with change and delete (with confirmation); a scan lists at most 15 networks, one entry per SSID, strongest first, sanitized, 4 per page, hidden ones via `Other...`; passwords on the on-screen keyboard (8–63 printable characters or 64 hex digits as PMK, E-510; the AP password 8–63; show/hide, counter), an open network only after a warning; a stored key is never shown. A status line gives the WiFi state and the reason of the last failure (wrong password, not found). With two stored networks the strongest seen is tried, but after a failure the other one if it is seen too (E-502, E-507, A-709).
- **A-505** Access point page: QR code `WIFI:T:WPA;S:…;P:…;;` (open: `T:nopass`, special characters escaped) on a white quiet zone, name, password in clear, address; the random password exists from the first visit on (bootloader entropy source while the radio is off); `Regenerate`, own password 8–63 characters, open only after a warning and switchable back; a running AP restarts with new data without touching the client connection. An open fallback AP starts only after the touch confirmed it (E-503, A-701, A-702).
- **A-506** Device page: device name (RFC 1123 label, 1–32 characters `a-z0-9-`, announced as `<name>.local`) and IP mode DHCP/static with address, mask, gateway, DNS (empty = gateway); switching to static prefills empty fields from the DHCP lease; an implausible combination (mask not /1–/30, address or gateway missing, address = gateway, gateway outside the subnet, network or broadcast address) is refused with the reason and the stored value stays. Changes at the touch apply at once; the two-stage change with fallback (A-712) applies to remote changes (E-504, A-712). The web password (8–64 characters) is set, changed or removed there (removing after a confirmation); it is never shown, hashed outside `loop()` with a salt from the true random source, and every change ends all web sessions without restarting the WiFi. The info card shows the remote clients and can disconnect all of them (E-623).

### 5.2 Decisions

#### E-501 WiFi modes
*(WEB-HMI concept.)* `Off` (default) / `Client` / `Access point`. A client without a connection additionally starts the AP after 60 s and keeps searching; an open fallback AP starts only after a confirmation at the touch screen (audit S-3).

#### E-502 Entering credentials
*(WEB-HMI concept.)* At the device (scan list, on-screen keyboard) or by phone over the AP; passwords never over an **open** AP (S-1, A-701).

#### E-503 AP password
*(WEB-HMI concept.)* A random WPA2 password per device (12 characters without confusable ones), created when the AP first starts and stored in NVS; shown on the display as text and **QR code**; it can be regenerated or chosen freely; open only on request with a warning (S-2).

#### E-504 IP configuration
*(WEB-HMI concept.)* DHCP; static IP optional with a plausibility check (S-15).

#### E-505 Device name
*(WEB-HMI concept.)* Default `cyd-hf-<4 hex digits of the MAC>`, RFC 1123 only (a–z, 0–9, `-`, ≤ 32); used for mDNS, the DHCP host name and the AP SSID `CYD-HF-<4 hex digits of the MAC>` (S-26).

#### E-506 Captive DNS only in AP mode
*(WEB-HMI concept.)* Captive DNS runs only in AP mode; no WPA2-Enterprise.

#### E-507 Stored networks
*(WEB-HMI concept.)* At most two; the strongest wins; minimum security WPA2, PMF "capable"; a warning for an open network (S-28).

#### E-508 RF purity: WiFi off by default
*(WEB-HMI concept.)* WiFi can be switched off and is off by default; `Off` stays the setting for the cleanest RF (E-512).

#### E-509 No WiFi setup while flashing
*(Decided by the user.)* No Improv Serial in the web flasher (the former step G3 was dropped); WiFi is set up only in the device menu, default `Off`.

#### E-510 WiFi key stored as passphrase, not as PMK
*(Step S52b, decided by the user.)* The concept (A-714) stored WPA2 keys only as PMK, so a flash dump would not reveal a passphrase used elsewhere. On the office network (WPA2/WPA3 transition mode) a PMK never connected (reason 2 and 202, AUTH_EXPIRE/AUTH_FAIL): ESP-IDF 5.5 always chooses WPA3-SAE when the AP offers it, and SAE needs the passphrase; `pmf_cfg.capable` is deprecated and SAE is compiled into the precompiled IDF, so neither can be switched off. With the passphrase the same network connected in 3.4 s. WPA3 and transition networks are the normal case on current routers, so the key is stored as entered (passphrase, or a PMK typed as 64 hex digits for pure WPA2 networks). *(AF8, audit DOC-13):* the touch keyboard refused the PMK while REST took it; now one rule (`NetConfig::validNetworkKey`) applies at the touch keyboard (64 characters for a network key), in REST and in the web UI: 8–63 characters of passphrase or exactly 64 hex digits. It is readable with a flash dump, as everything else without flash encryption (S-22); it is never shown, logged or exported, the interfaces only say set/not set. The PBKDF2 code stays for the web password hash (A-713). Measured on the CYD2USB with a phone hotspot: fallback AP after 61.9 s without connection, AP off 0.1 s after the client got its address (E-511, log resolution 0.1 s); flash +660 KB for the WiFi stack (1 352 091 B, 43.0 %), RAM 124 220 B.

#### E-511 No routing between AP and client
*(Audit S-16.)* The official platform with the precompiled IDF has `CONFIG_LWIP_IP_FORWARD=y` built in. Instead of an own IDF build (pioarduino with `custom_sdkconfig`: own IDF build, longer builds, a second platform source) A-703 is applied strictly: AP and client never have an address at the same time; the AP goes off as soon as the client has its address, checked within ≤ 1 s (measurement M-211). The WiFi manager switches the AP off in `WifiManager::update()` from `loop()`, not in the IP event, so the limit depends on the `loop()` latency.

#### E-512 WiFi without power save, full TX power
*(Step AF55, audit PERF-15.)* Arduino-ESP32 starts the client with modem sleep (`WIFI_PS_MIN_MODEM`): the radio wakes only for the beacons, so in the LX1 device test a ping took 220–390 ms and pyvisa-py ran into its default connection timeout. Response time comes first for a remote-controlled instrument, so the WiFi manager sets `WIFI_PS_NONE` (stored in the core, which applies it at every STA start, and set in the driver after every start of client, access point or the scan-only client) and the highest TX power level (`esp_wifi_set_max_tx_power(84)`, limited by the driver to the calibrated 20 dBm). Cost: about 80–100 mA more supply current while WiFi is on (data sheet, not measured) and a radio that is active all the time next to the SI5351; WiFi `Off` stays the setting for the cleanest RF (E-508). Measurements and the changes they lead to come last; the measurement task MES-03 compares `PS_NONE` with `MIN_MODEM` and a reduced TX power on the spectrum; a change after it gets its own decision.

#### E-513 WiFi search with backoff
*(Step AF14, audit PERF-5.)* The client scanned actively every 15 s while it had no address, also without any stored network and next to the fallback AP, which leaves its channel for every scan (about a quarter of the time with the 15 s attempt that followed), and `WiFi.begin()` for an unseen stored network (hidden SSID) followed every scan. Now there is no background scan without a stored network; the interval starts at 15 s and doubles after every scan without a stored network up to 5 min, a seen network or a connection resets it, and with stations at the fallback AP it is at least 2 min. The attempt for a hidden network follows the scans and so runs at the same pace. Price: reconnecting after a router outage can take up to 5 min (accepted by the user); a scan of the WIFI page or new settings start again at 15 s. The timing is pure and host-tested in `WifiPolicy::Connector` (AF13).

#### E-514 One owner of the network settings during a web change
*(Step AF16, audit concept 2, audit CQ-2.)* A web network change kept the previous settings for up to 120 s while the WIFI pages stayed usable: a revert threw away every touch change of that time (also a new web password), and a touch change saved the unconfirmed web state, which then applied after a power loss (against A-712). Of the three options (lock the touch, a three-way merge, one owner) the plan recommended one owner with the semantics of the lock: `NetChange` (pure, `net/web_state.*`) owns the settings while a change is open. The touch path (`Gui::commitNet_`, AF15) refuses with `Web change pending` and offers `Revert` and `Keep`; the WiFi manager does not save its own change (a created AP password) until the confirmation, which saves the manager's settings; only a confirmed or final state is written, so `Revert` only applies the previous settings (NVS still holds them). A three-way merge was rejected because mode, AP password and web password depend on each other. With a web password the touch need not confirm, but the device shows the open change with `Revert`. Host tests: touch change in the window, revert before the change applied, timeout, confirmation, power loss at every moment = NVS content.

#### E-515 Web network changes: two-stage, Off final
*(Step S55, audit SEC-2, AF2.)* A network change from the web is applied, then has to be confirmed within 120 s over the new connection (A-712); without a web password also with `Keep` at the touch screen, because anyone in the network could otherwise move the device away. `Off` cannot be confirmed over a new connection and is the safe state, so it applies at once and is final; an automatic return would switch the radio on against the user's will. Because it is final, `Off` takes only the mode; a body with any other key is refused with `-108` (400, `Parameter not allowed`) and changes nothing, otherwise networks, an open AP, the device name or a static IP from anyone in the network would be saved without the confirmation and take effect when the owner switches WiFi on again at the device. Other changes come first as a confirmed change of their own.

---

## 6. Remote control (SCPI, LXI/VXI-11, REST, WebSocket)

### 6.1 Command core

- **A-601** Every controller setter reports its result (`Ok`, `NoChange`, `OutOfRange`, `Invalid`, `Conflict`, `Denied`, `HwError`); values are checked in their full type and never cut or clamped silently, only the `adjust*`/step functions of the GUI clamp to the range; the channel index is 1–2 (frequency) or 1–3 (output) strictly (V-1, S-12, E-607).
- **A-602** No network task calls `AppController`, LVGL, I2C or NVS directly; writes go through the command queue, reads through the state snapshot (with a sequence counter, because uint64/double are not atomic).
- **A-603** Foreign strings reach LVGL and JSON only through `sanitize`; passwords are never output (only "set / not set").
- **A-604** JSON nesting ≤ 10 (ArduinoJson 7.4.3); SCPI waits for acknowledgements at most 1 s.
- **A-605** The security logic (`request_guard`, `sanitize`, `line_framer`, SCPI core, JSON mapping) is free of Arduino and web server dependencies, host-tested and fuzzed (A-725).
- **A-606** With WiFi off no network task runs (`WIFI_OFF`); SCPI over USB stays.

### 6.2 SCPI

- **A-607** SCPI over the USB serial port (115 200 baud, LF) with the command tree of the manual (section 15, authoritative: short form upper case, `[ ]` optional, `<n>` channel suffix, numbers and error codes): IEEE 488.2 common commands, outputs, frequencies, CH3, all CONFIG settings except `Remote outputs` and the network settings, which SCPI cannot change; diagnosis, log, save, identify. Commands go through the command queue and the controller like touch actions; queries read the state snapshot (V-6).
- **A-608** Numbers are parsed exactly as integers (Hz, 0.01 ppm, %), with the units HZ/KHZ/MHZ, PPM, PCT, DEG, exponents and MIN/MAX/DEF; nothing is clipped: out of range −222, NAN/INF/hex/unknown units/values not in the set −224, wrong data type −104. A command with an extra parameter (−108) or any error changes nothing (E-607, A-719).
- **A-609** A line has at most 256 bytes, 16 commands and only printable ASCII or TAB; otherwise it is discarded up to its LF without executing anything (−363, −101, −223) (A-717).
- **A-610** At most 20 setting commands per second and source; further ones are refused (`-200,"Execution error;Too many commands"`) (A-711).
- **A-611** The first line over USB starts an SCPI session: log lines go only into the ring buffer (A-407) and the Arduino core's own messages are off until 10 min without a line (E-612, A-722). Opening the port with DTR/RTS off does not restart the device.
- **A-612** `Remote outputs` (E-610, A-721): `allowed` (default) or `local only`; with `local only` a remote source cannot switch an output on or select off state `High` (`Denied`, SCPI −221, REST 403; E-611), switching off and the off states `Low` and `High-Z` stay allowed. Only the touch source changes it; it is saved at once, kept by `Defaults` and `*RST` and reset by the factory reset.
- **A-613** `SYSTem:IDENtify` blinks the backlight and the RGB LED for 5 s (E-613).
- **A-614** SCPI over TCP on port 5025 while the client is connected or the AP runs with a password (never over an open AP), announced as `_scpi-raw._tcp`: one connection (a second one is closed without an answer), own error queue and status registers cleared per connection, the same line limits as USB; a line that looks like an HTTP request closes the connection before anything of it runs (A-718); idle 10 min and keepalive 60/10/3 s close it; answers are sent per line without blocking `loop()`, a peer that does not read is closed when the buffer is full. Commands are never logged (E-601, E-603, A-720). No authentication: the web password does not apply, every command (also `*RST`, `SYSTem:SAVE`) is open to anyone who reaches the port; protection is `Remote outputs: local only` and a trusted network (E-704).

### 6.3 LXI and VXI-11

- **A-615** LXI discovery and VXI-11 (step LX1) under the same gate as SCPI over TCP (A-614; one condition switches both): a portmapper (program 100000 version 2) on UDP and TCP 111 answers NULL and GETPORT, also to broadcasts (`lxi discover`), and DUMP over TCP only; GETPORT knows only the VXI-11 core program (0x0607AF v1 TCP) and the portmapper itself, other procedures give PROC_UNAVAIL, rpcbind versions 3/4 PROG_MISMATCH 2..2; over UDP no reply is larger than its request and at most 20 replies per second are sent; over TCP two connections at a time, a further one replaces the one idle the longest. The VXI-11 core channel on TCP 9009: one connection with one link (`inst0`), create_link, device_write, device_read, device_readstb, device_clear, device_lock/unlock, device_remote/local and destroy_link on an own SCPI session (source SCPI, line limits of A-717, `Remote outputs` applies, error queue cleared per link), END flag and termination character, maxRecvSize 1024, records up to 2 KB (a larger one closes the connection), idle 10 min and keepalive as A-614; device_trigger, device_docmd, SRQ and the interrupt channel answer error 8, no abort channel (abortPort 0). mDNS announces `_lxi._tcp` (port 80, TXT with manufacturer and model only, A-716), `_vxi-11._tcp` (port 111) and `_http._tcp` besides `_scpi-raw._tcp` (E-615); `GET /lxi/identification` returns the LXI identification XML through the request guard (Host check as for the pages), with the `*IDN?` serial number and without MAC or commit (E-616), readable without login also with a web password (E-614). No authentication and no setting of its own (as A-614, E-704); calls are never logged (E-614).

### 6.4 Web server, REST and WebSocket

Operation in manual section 16; REST contract, error codes and the keyboard of the web UI (`PgUp`/`PgDn` = frequency × 10 / ÷ 10) in [data/www/README.md](data/www/README.md), which fixes the points A-617 to A-627 under [Settled in S55](data/www/README.md#settled-in-s55-firmware--reference-the-mock-follows-it).

- **A-616** Web server on port 80 while the client is connected or the AP runs: the embedded web UI and the REST API/WebSocket of this section. Every request passes the request guard (host allowlist without IPv6, origin, `Sec-Fetch-Site`, token, content type, session) and the A-710 limits on the raw bytes before parsing (URL 256 B, headers 2 KB, body 1 KB, 10 s per request, at most 4 HTTP connections and 3 WebSockets, 24 KB free heap and an 8 KB free block for a new connection); WebSocket pushes at most every 100 ms and only with a free block for them; security headers and CSP without inline code. Writes answer with a state that contains them. The web password check (PBKDF2) never runs in the network task; 5 failures lock the login for 30 s, doubling up to 960 s. A network change from the web stays only when confirmed within 120 s over the new connection and, without a web password, also at the touch screen; `Off` is final. With a web password reading needs the login as well (state, token, WebSocket; only the page and the LXI identification stay open, E-703, E-614). While it is open only the confirmed state is saved (after a power loss the previous settings apply), the touch screen changes no network settings and offers `Revert` (E-514). Password fields are refused over an open AP, in every POST body including the login (A-701, A-704 to A-713, A-723).
- **A-617** The state contains `wifi.confirmWithin` (seconds, 0 = nothing open).
- **A-618** Mode `Off` over the web is final (no confirmation over a new connection is possible, E-515).
- **A-619** The state contains `device.passwordSet`; the web password is set only at the device.
- **A-620** A stale token gets the fixed text `-200,Invalid token: reload the page`.
- **A-621** Error codes: invalid JSON −102, wrong type −104, unknown key −108, outside the value lists −224; the meaning of `modes` and `actual` is fixed in the contract.
- **A-622** Scan: a `GET` starts it, the first `GET` after the end gives `done`, a new start within 10 s → 429.
- **A-623** Without a web password `/api/state` and `/ws` are readable without login; with a web password only with a session, as is `/api/token` (E-703); `/lxi/identification` stays open (E-614).
- **A-624** The WebSocket limit of ≤ 512 B (A-706) applies only to incoming frames.
- **A-625** A `frequencyText` outside the range gives −222.
- **A-626** Without a connection `wifi.ssid` and `ip` are `""` and `rssi` is 0; with the AP `ip` is the AP address.
- **A-627** Short error texts, status codes 403/429/503/500/413.

### 6.5 Decisions

#### E-601 Protocols: SCPI, LXI/VXI-11, REST
*(WEB-HMI concept.)* SCPI (IEEE 488.2 / SCPI-99, own tree) over TCP 5025 (mDNS `_scpi-raw._tcp`) and USB; with step LX1 also VXI-11 (portmapper UDP/TCP 111, core channel, mDNS `_vxi-11._tcp`) and LXI discovery (`_lxi._tcp`, `/lxi/identification`), which replaced the former "no VXI-11" (identification in the web server without login, E-614); no HiSLIP, no MQTT.

#### E-602 SCPI on scpi-parser with own number parsing
*(Step S51.)* The command tree runs on j123b567/scpi-parser (BSD-2): header matching with short/long forms and suffixes, error queue and IEEE 488.2 registers are tested there and fuzzed (issue #174: about 95 M executions of lexer and parser clean; the three findings are in the example code, which is not used). Pinned to commit `886615902b2777b71860445305bfe5a3faef4f3e` = v2.3 plus `strings.h` for `strncasecmp` and a `-Wdouble-promotion` fix. Configured identically for the ESP32 and the host (`include/scpi_user_config.h`): full error list, error info in a fixed 256-byte buffer per session (no malloc), LF line ending. Numbers are not converted through the library's `double`: `remote/scpi.cpp` parses the token text into exact integers, so `7.1E6` and `10.0000005 MHZ` give 7 100 000 and 10 000 001 Hz without binary rounding. Two deviations from the concept: the library knows no optional first node, so `[SOURce<n>:]` is listed twice; the actual output state is `OUTPut<n>:CONDition?` instead of `:STATus?`, because the short form `STAT` would also match `STATe`. `SYSTem:LOG?` returns one line per query, so line-based clients need no block format. Measured on the CYD2USB (Arduino 3.3.12): 1000 × `*IDN?` without error, median 6.0 ms, 99 % below 11.6 ms; flash +24 KB, RAM +1.3 KB. Over TCP on the office WiFi (step S54): median 14.7 ms, 99 % 97 ms, max. 1.4 s (WiFi latency peaks); heap after an 8 s flood 133 KB free, minimum 81 KB.

#### E-603 One SCPI connection
*(WEB-HMI concept.)* One SCPI connection; a second one is refused; idle timeout 10 min; `Disconnect` at the device (S-17).

#### E-604 Concurrent operation: the last one wins
*(WEB-HMI concept.)* Touch, web and SCPI may operate at the same time without restriction; the last change wins; the status bar shows `REMOTE <source>` (S-18).

#### E-605 Events by polling
*(WEB-HMI concept.)* SCPI clients poll and use `*OPC?`; live push only over the WebSocket.

#### E-606 Connection loss keeps the outputs
*(WEB-HMI concept.)* When a remote connection drops, the outputs keep running as set.

#### E-607 Limits as at the touch screen
*(WEB-HMI concept.)* Remote input has the same limits as the touch screen; every value is checked in its full type and never cut silently (S-12).

#### E-608 `*RST` does not save
*(WEB-HMI concept.)* `*RST` switches the outputs off and sets the defaults in RAM; nothing is saved until `SYSTem:SAVE`; a later change saves only its own key (C-17).

#### E-609 Remote access without SI5351
*(WEB-HMI concept.)* Web and SCPI run without SI5351; the channels report `NO HW`.

#### E-610 Remote switching: `Remote outputs`
*(WEB-HMI concept.)* CONFIG `Remote outputs: allowed / local only`, default `allowed`, changeable only at the device; under `local only` the network may set everything but switch nothing on (S-18).

#### E-611 Remote off state High only with remote outputs allowed
*(Step AF1, audit SEC-V-1, decided by the user.)* Off state `High` puts the output supply (about 3.3 V, current limited by the drive strength) on every switched-off output, including CLK2; for a DC-coupled diode ring mixer that is as harmful as a running signal. The confirmation of A-310 exists only at the touch screen and in the web UI; SCPI and REST set it at once. `Remote outputs: local only` promises that nothing reaches the outputs from the network, so `AppController::setOffState()` refuses `High` from a remote source under `local only` with `Denied` (SCPI and VXI-11 `-221`, REST 403), checked before `NoChange` like switching on. With `allowed` it stays allowed without a question at the device: the network may then switch the outputs on as well, and a confirmation at the touch screen would block remote-only setups. The web UI skips its own question under `local only`, because the device refuses anyway. Flash +68 B (1 518 234 B), static RAM unchanged (78 432 B).

#### E-612 No log on the serial port during an SCPI-over-USB session
*(WEB-HMI concept.)* While an SCPI session runs over USB, no log line goes to `Serial` (ring buffer, `SYSTem:LOG?`) (C-16).

#### E-613 Identify
*(WEB-HMI concept.)* The web button and `SYSTem:IDENtify` blink the backlight and the LED for 5 s.

#### E-614 VXI-11 with an own portmapper; core channel on fixed port 9009
*(Step LX1.)* LXI tools find instruments by a portmapper GETPORT broadcast to UDP 111 (lxi-tools `lxi discover`, pyvisa-py `list_resources()`) and reach `TCPIP::<ip>::INSTR` through the portmapper on TCP or UDP 111; mDNS (`lxi discover -m`) lists the device but gives no VISA client the core port. The device therefore runs a minimal portmapper of its own (version 2, three fixed mappings) instead of rpcbind: no SET/UNSET (nothing registers at run time) and no CALLIT, which would forward calls and make the device a UDP reflector; over UDP a reply is never longer than its request and at most 20 replies per second leave. rpcbind versions 3 and 4 get PROG_MISMATCH 2..2; libtirpc (lxi-tools on Linux and Windows) then keeps that TCP connection to port 111 open and asks GETPORT version 2 on a second one, so the portmapper takes two TCP connections, and a third replaces the one idle the longest (it is stateless; an idle client cannot lock it). A one-connection portmapper made `lxi scpi` fail with "Unable to connect" (found in the interop test, host test `test_portmap_tcp_pool`). The core channel listens on the fixed port 9009 instead of an ephemeral one: one listener for the whole uptime, stable firewall rules, a documented port. HiSLIP was not chosen: lxi-tools, pyvisa-py and python-vxi11 all speak VXI-11, HiSLIP needs two channels per session. Limits as for SCPI over TCP: one connection with one link, records up to 2 KB (maxRecvSize 1024; the largest device_write with AUTH_SYS credentials fits), answers up to 2 KB per line, reads up to 1 KB; no abort or interrupt channel, `device_trigger` unsupported (no trigger system). A read with no answer waiting gives error 15 at once: commands run synchronously in `loop()`, so waiting for io_timeout could only block. A new message while an answer is unread discards it with -410 (IEEE 488.2); `device_clear` keeps the error queue (DCL). Parsing and encoding are pure and host-tested (`onc_rpc`, `vxi11`, test_vxi11) and fuzzed (`vxi11`, 1 000 000 standalone inputs clean); the lwIP glue reads at most up to the end of a record, so at most one reply waits. The host test found a 32-bit wrap in the XDR opaque length check (`0xFFFFFFFF + 3`), fixed before it shipped. No authentication and no setting of its own, like SCPI over TCP (audit SEC-1/DOC-1, E-704: usual for lab instruments, protection by a trusted network and `Remote outputs: local only`); both use one gate (`ScpiTcpLink::allowedFor` in `pollNetworkScpi()`), never over an open AP. `GET /lxi/identification` is served by the web server on port 80 (Host and Origin checks as for the rest of the site) and stays readable without login also when a web password is set (decided by the user): it holds only what `*IDN?` gives over network SCPI anyway, and LXI tools read it without credentials; it is the exception to E-703; the request guard asks a session for reads only under `/api/` and `/ws`, `test_http_guard` pins the exception. Checked on the host with the firmware logic behind a socket shim on 127.0.0.1: lxi-tools 2.8 for Windows (`lxi scpi`, `-x`, `lxi benchmark -c 500`), python-vxi11 0.9, PyVISA 1.16.2 with pyvisa-py 0.8.1 (query, read_stb, clear, lock, timeout, the pyvisa-py broadcast client) and `tools/lxi_example.py`; `lxi discover` found the fake device (`tools/fake_scpi_device.py --vxi11`); `lxi discover -m` and the device itself are still open. Flash +10.5 KB (1 518 166 B against 1 507 646 B), static RAM unchanged (78 432 B), heap about 8.5 KB for the server object from boot plus 1.3 KB per identification request while it is sent.

#### E-615 mDNS announces SCPI and LXI also while blocked at an open AP
*(Step LX1, decided by the user.)* mDNS announces `_scpi-raw._tcp`, `_vxi-11._tcp` and `_lxi._tcp` also while network SCPI is blocked at an open AP; no withdrawal (an open AP usually serves just one device for the setup).

#### E-616 LXI identification without MACAddress and LXIVersion
*(Step LX1, decided by the user.)* `/lxi/identification` carries no `MACAddress` and no `LXIVersion` (A-716, no claim of conformance) and is therefore not strictly schema-valid; lxi-tools, PyVISA and python-vxi11 do not need them.

#### E-617 Optional web password with session login
*(WEB-HMI concept.)* The web password is optional; with one, login with a session cookie (no basic auth), salted hash, lock after failed attempts (S-9).

#### E-618 Scope of the web interface
*(WEB-HMI concept.)* All functions including CONFIG and WiFi, except `Remote outputs` (E-610); network changes in two stages (confirmation over the new connection within 120 s, otherwise fallback; without a web password also at the touch screen, S-15).

#### E-619 At most three web clients
*(WEB-HMI concept.)* At most 3 web clients, synchronized live; a fourth one gets an error page.

#### E-620 Web UI language and resources
*(WEB-HMI concept.)* English, responsive, only local resources.

#### E-621 Web UI embedded in the firmware
*(Step S55.)* The web UI (50 KB, 19 KB gzip) is gzipped at build time (`tools/embed_www.py`) and linked into the firmware instead of a LittleFS image: one image for the web flasher and later updates (step G2), no file system to keep in step with the firmware, ETag from the content. Flash +125 KB with the server, RAM +0.7 KB.

#### E-622 REST writes and WiFi jobs wait for loop() without polling and are answered later when it is slow; JSON without allocations
*(Step AF54, audit PERF-7, PERF-9, A-723.)* A REST write or a WiFi job (change, confirmation, scan) blocked the `async_tcp` task for up to 80 or 180 ms, checking the result every 1 ms, and every state JSON built an ArduinoJson document with about 20–30 heap allocations. Now the handler waits at most 20 ms and is woken by `loop()` once per pass (a binary semaphore given after the commands became visible and the job was served), which covers the usual loop pass; a slower `loop()` (SI5351 reconnect, NVS write, a full redraw) gets the request paused (`AsyncWebServerRequest::pause()`) and answered from the connection's poll callback, a write once its commands are visible or, at the first poll after 80 ms, with the state as it is (as before), a job once `loop()` finished it or with 503 at the first poll after 180 ms. Answering only from the poll callback, as first planned, was not chosen: it runs every 500 ms (lwIP slow timer, `CONFIG_ASYNC_TCP_POLL_TIMER` 1), so every write would have waited up to 0.5 s and the web UI, which sends the next coalesced step only after the answer, would have tuned a held `+` at about 2 steps/s instead of 10–20. Sending the answer from `loop()` was not chosen either: the library's response code is not thread-safe against the ACK handling in `async_tcp`. The state JSON and the error bodies are written by `JsonWriter` into the caller's buffer (no heap), byte-identical to the former ArduinoJson output: `test_json_writer` compares both for 20 000 random states with foreign texts of any byte value and every buffer size around the length. ArduinoJson stays for parsing request bodies.

#### E-623 `Reconnect all` instead of `Disconnect all`
*(Decided by the user.)* The button for the remote clients is called `Reconnect all`: it disconnects all remote clients, which then connect again by themselves; no lock-out time; locking out is done with the web password or `Remote out.`.

---

## 7. Security

The security requirements of the network functions (formerly the W-S requirements of the WEB-HMI concept; the audit finding of each is in parentheses). Network code is reviewed against them (ARBEITSPLAN rule 0.2).

### 7.1 Requirements

- **A-701** Over an open AP the server refuses WiFi and web password fields (403); the default is the WPA2 AP with a random password; WiFi passwords never travel over an open AP. (S-1, S-2)
- **A-702** An open fallback AP starts only after a confirmation at the touch screen; an open AP without a station goes off after 15 min, takes ≤ 2 stations, status bar `AP open`. (S-2, S-3)
- **A-703** No routing AP ↔ STA: NAPT off, DHCP without gateway, AP and STA never both with an address (AP off in the event of the STA address, ≤ 1 s), because the precompiled IDF has `IP_FORWARD` switched on (E-511). (S-16)
- **A-704** Host allowlist for HTTP and WebSocket (own IPv4, `<name>.local`, `<name>`, in AP mode for the captive DNS), otherwise 421; IPv6 off. (S-5)
- **A-705** POST only with `application/json` (415), a valid `X-CYD-Token` (403), `Origin` empty or the own host, `Sec-Fetch-Site` not `cross-site`; no CORS headers, `OPTIONS` refused, GET without effect. (S-4)
- **A-706** WebSocket upgrade with host, origin and token check (with a web password also the session, E-703); push only; ≤ 512 B; ping every 15 s with a random payload, close after 30 s without a matching pong; a client with > 3 pings in 10 s is closed (AF18). (S-10)
- **A-707** Headers: CSP `default-src 'none'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'`, `nosniff`, `Referrer-Policy: no-referrer`; `/api/*` with `no-store`. (S-7)
- **A-708** Foreign strings in the browser only through `textContent`/DOM, never `innerHTML`; JSON only through the JSON layer (ArduinoJson for parsing, `JsonWriter` for writing, E-622). (S-7)
- **A-709** Every foreign string passes `sanitize` before LVGL and JSON; scan list ≤ 15 entries, no recolor; an LVGL assert restarts the device. (S-14, C-3)
- **A-710** Body ≤ 1 KB (413), multipart/chunked 415, URL ≤ 256 B, headers ≤ 2 KB, 10 s timeout; ≤ 3 WebSocket and ≤ 4 HTTP connections; < 24 KB free heap or a largest block < 8 KB → 503 for new connections (protection limit; `SYSTem:HEAP?` shows the values); push at most every 100 ms (AF19). (S-13)
- **A-711** ≤ 20 write commands per second and source (429), a scan at most every 10 s, after 5 login failures a 30 s lock (doubling). (S-9, S-13)
- **A-712** Network changes in two stages with fallback after 120 s, without a web password also at the touch screen; only a confirmed state is saved, during the wait no network change at the touch screen (`Web change pending`, `Revert`/`Keep`, E-514); a static IP differs from the gateway, lies in the subnet and is neither network nor broadcast address. (S-15)
- **A-713** Web password ≥ 8 characters, PBKDF2 hash (mbedTLS); cookie `HttpOnly; SameSite=Strict`, 12 h; broken `Authorization`/`Cookie` headers are refused. (S-9)
- **A-714** WiFi key stored as passphrase (E-510: WPA3-SAE needs it; before: PMK), `WiFi.persistent(false)`; the factory reset erases `hfnet`, `nvs.net80211` and the coredump; passwords never appear in any output. (S-8)
- **A-715** STA with PMF "capable" and at least WPA2; AP as WPA2/WPA3-SAE transition if stable (otherwise WPA2). (S-28)
- **A-716** Device name RFC 1123, AP SSID printable ASCII ≤ 32 B; mDNS TXT without version and commit. (S-26)
- **A-717** SCPI lines ≤ 256 B, overlength discarded up to `\n` (−363), ≤ 16 commands per line, NUL/non-ASCII discards the line. (S-11)
- **A-718** SCPI over TCP closes the connection without execution on HTTP-like lines (`^[A-Z]+ \S+ HTTP/1\.[01]`, `^[A-Za-z0-9-]+:\s`). (S-6)
- **A-719** Parameters are checked in their full type (finite, integer, in range) before they are narrowed; frequencies are integers in Hz. (S-12, C-4)
- **A-720** SCPI: idle 10 min, TCP keepalive, IP on the info card, `Disconnect` at the device; off over an open AP. (S-17)
- **A-721** `Remote outputs: local only`: switching on from web/REST/SCPI is refused (403 / −221), switching off is allowed, the option changes only at the touch screen; status bar `REMOTE <source>`. (S-18)
- **A-722** During an SCPI session over USB no log lines on `Serial`. (C-16, S-25)
- **A-723** `loop()` is supervised by the task watchdog; no network task blocks > 100 ms; stack reserve ≥ 1 KB after 24 h. (S-23)
- **A-724** Dependencies pinned exactly, advisory check per release; CI with SHA-pinned actions and `permissions: contents: read`. (S-20, S-27)
- **A-725** Fuzzing of `line_framer`, SCPI core, `request_guard`, `sanitize`, JSON mapping, VXI-11 with portmapper and RPC record marking (`vxi11`, step LX1): libFuzzer with ASan/UBSan ≥ 10 million executions before every release; a random test with a fixed seed (100 000 inputs) in `native-win`. (S-11, S-13)
- **A-726** Operating limit: in the worst legitimate load (3 WebSockets, 4 HTTP connections, an SCPI flood over TCP and an SCPI session over USB at the same time) at least 40 KB of heap stay free and the largest free block is at least 16 KB, so full normal use never reaches the protection limit of A-710 (24 KB / 8 KB). Below 40 KB free heap the firmware logs a warning `Heap low` once until the heap recovers. A measurement below the limit lowers the memory use (e.g. the `async_tcp` stack), not the limit. (S-13)

### 7.2 Decisions

#### E-701 No TLS, no Secure Boot, no flash encryption
*(WEB-HMI concept; audit S-22.)* TLS/HTTPS is not used (no certificate can be managed on the device), nor Secure Boot and flash encryption. The manual names the consequences: plain text on the network, a guest or IoT WiFi is recommended, physical access = full access. HTTPS stays a later option (E-101).

#### E-702 Request guard on 64-bit time; raw-byte pre-filter
*(Steps S55, S57c.)* Fuzzing showed that sessions and login locks compared with 32-bit `millis()` differences came back after the wrap (49.7 days); the guard now takes 64-bit monotonic milliseconds (`esp_timer`). ESPAsyncWebServer stores header lines without a limit, so the A-710 limits are checked on the raw bytes (`onPacket`) before the library parses them; this relies on AsyncTCP internals, so the libraries stay pinned exactly (ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0).

#### E-703 With a web password reading needs the login; WebSocket control frames bounded
*(Step AF18, audit SEC-5, SEC-V-2, PERF-18, decided by the user.)* The token is in the page, so anyone who could load the page could open the three WebSockets with it and lock out the owner (503), also with a web password; the state was readable for everyone. With a web password the request guard now asks a session also for `/api/state`, `/api/token` and the `/ws` upgrade (`loginForReads`, which existed since step S55 for this case), and the page asks for the password when it opens; the page and `GET /lxi/identification` stay open (E-614: it holds only what `*IDN?` gives over network SCPI without login, E-704). Without a web password nothing changes. This changed point 7 of the S55 contract (A-623). A password change closes every WebSocket so that it comes back under the new rule. ESPAsyncWebServer answers every PING with a PONG in an unbounded control queue (a PING flood fills the heap until a restart) and passed any PONG as liveness, so a client that reads nothing could keep its slot: the device now pings with a random 4-byte payload, counts only the PONG that echoes it, and closes a client after more than 3 PINGs in 10 s (the UI never pings); the web UI closes its WebSocket after 60 s hidden and sends no heartbeat in the background. Evicting a live client for a new one was not chosen: E-619 stays (a fourth sees `Too many clients`), and with a password only logged-in browsers hold slots. The PING flood should also be reported upstream (ESPAsyncWebServer, task USR-04). Host tests in `test_web_state`; `web_check` covers this, the open identification and the PING flood against the mock.

#### E-704 Network SCPI without authentication
*(Step AF4, audit SEC-1, decided by the user.)* SCPI over TCP 5025 and VXI-11 take every command from anyone who reaches the device; the web password (A-713) protects only the web interface and REST. Lab instruments with LAN (raw socket, VXI-11, LXI) work this way, the tools (lxi-tools, PyVISA, python-vxi11) have no login of their own, and a password sent over plain TCP would protect little anyway. A setting "network SCPI off / read only / on" (audit concept 1, AF20) was dropped by the user: protection is `Remote outputs: local only` (no switch-on and no off state High from the network, E-611), no SCPI over an open access point (A-614, A-615) and a trusted network, best an own guest or IoT WiFi; this is documented clearly in the manual and `SECURITY.md`. The audit found the manual implied that a web password locks out remote access; manual sections 8, 14, 15 and 16 and `SECURITY.md` now say that it does not. A client may hold the single SCPI connection as long as it sends lines; `Reconnect all` at the device frees it.

---

## 8. Build, tests, supply chain, release and CI

### 8.1 Requirements

- **A-801** PlatformIO + Arduino-ESP32. `esp32-2432S028R` is the only firmware environment and `default_envs`; `native` (gcc in PATH) and `native-win` (bundled MinGW) run the host tests.
- **A-802** Pinned versions: pioarduino platform `55.03.313` by release URL (Arduino-ESP32 3.3.13, ESP-IDF 5.5.6, framework packages by release URL with their SHA-256 checked in CI, built with `-Os`, E-803, E-804), `bodmer/TFT_eSPI@2.5.43`, `lvgl/lvgl@8.4.0` (E-304), XPT2046_Touchscreen from GitHub, pinned to commit `d57f64c8b5f2bc5b8d10d121550806eeff7b06d9` (tag v1.4, E-802), j123b567/scpi-parser pinned to commit `886615902b2777b71860445305bfe5a3faef4f3e` (E-602), `bblanchon/ArduinoJson@7.4.3`, `esp32async/AsyncTCP@3.5.0` and `esp32async/ESPAsyncWebServer@3.12.1` (exact, E-702). Partition table `huge_app.csv`, 4 MB flash.
- **A-803** TFT_eSPI is configured only through build flags (`USER_SETUP_LOADED`), never by editing library files.
- **A-804** The host tests compile the hardware-independent sources (controller, SI5351 driver and planner, I2C bus, display detection, services, frequency utilities, logger) against fakes (E-801), and the remote layer (command queue, line framer, SCPI with the parser library, SCPI over TCP, VXI-11 and portmapper, REST JSON with ArduinoJson) and the network modules (network settings, PBKDF2, request guard, raw-byte limits, web state, scan list, WiFi policy); the glue to AsyncTCP, ESPAsyncWebServer and the WiFi driver is not host-tested. These sources stay free of LVGL and TFT_eSPI; `constants.h`, `types.h` and `frequency_utils.h` do not include `Arduino.h`.
- **A-805** The own sources (`src/`) compile with `-Wall -Wextra` (`build_src_flags`) without warnings; libraries keep their default flags.
- **A-806** The version is `APP_VERSION` in `config.h`. Branch, commit (with `+` for uncommitted changes) and build time are taken at build time; without git they read `unknown` (E-805, E-806).
- **A-807** Touch test builds (`TEST_TOUCH_LOG`, `TEST_HIT_MAP`, own PlatformIO envs) log every touch and show a hit map for the operating tests; they are never released, the release env contains none of it, and CI builds the hit map env.

### 8.2 Decisions

#### E-801 Host unit tests
*Decided.* Driver math and controller logic are tested without hardware. `test/fakes` replaces Arduino, `Wire` (SI5351 register model with fault injection) and `Preferences`; `test/helpers/si5351_model.h` decodes the written registers independently of the driver's arithmetic, so a wrong formula cannot confirm itself. A check with deliberately inserted faults (correction sign, phase offset −1) made the affected tests fail.

#### E-802 XPT2046 library from GitHub
*Original specification.* Installed from GitHub (tag v1.4, now pinned to its commit, A-802), because the PlatformIO registry version was unreliable.

#### E-803 Arduino-ESP32 core 3 only
*(Step S41d, decided by the user; supersedes the first platform decision of the work plan and of the WEB-HMI concept.)* The first plan made `espressif32@8.0.0` the official platform after the spike S37 and migrated to it before the WEB-HMI, with the heap watched continuously. Until then both cores were built: `espressif32@6.7.0` (Arduino 2.0.16, ESP-IDF 4.4.7, end of life, mbedTLS 2.28 end of life) as the release platform and `espressif32@8.0.0` (Arduino 3.3.12, ESP-IDF 5.5.5) until the measurement acceptance (task MES-02). All WiFi and remote work was developed and tested on core 3 only, and two cores doubled every build and check. Core 2 is therefore dropped before the acceptance: the env `esp32-2432S028R` is built on core 3 with `-Os` (without it Arduino 3.3.12 builds with `-O0` and LVGL draws 4x slower), the 2.0.x LEDC branch and its fake are removed. The smoke test on core 3 passed on both boards (step S41b); the acceptance measurements (MESSPLAN 5.10) stay open. Way back: the last release built on core 2. The core is updated within 3.x; since E-804 it is Arduino 3.3.13 on the pioarduino platform. espressif32 6.13.0 is not used.

#### E-804 Arduino-ESP32 3.3.13 / ESP-IDF 5.5.6 on the pioarduino platform
*(Step N11, decided by the user.)* ESP-IDF 5.5.5 has the lwIP DNS cache overflow GHSA-ch63-jxm5-vmvf (high, fixed in 5.5.6); the advisory check (AF26) blocks every release with it, and it is not accepted as an exception: the release waited for the update. The PlatformIO registry offers no platform with Arduino 3.3.13 / IDF 5.5.6, so the firmware is built on the community platform pioarduino `platform-espressif32` 55.03.313 (Apache-2.0), which installs Espressif's own release archives `esp32-core-3.3.13` and its libraries (built from ESP-IDF `release/v5.5` at `a45de8ccca1`, 436 commits after the tag v5.5.6, which it contains, including the DHCP fix CVE-2026-45160). Pinning: the platform and both framework archives by release URL; PlatformIO checks no checksum for URL packages, so `tools/dep_check.py` compares the SHA-256 digests GitHub publishes for these assets with recorded values (a replaced asset fails CI and the release); toolchain (GCC 14.4.0+20261030) and esptool 5.6.0 cannot be pinned in `platform_packages` (pioarduino installs them with `idf_tools.py`, with checksums from its `tools.json`, and an override hides the compiler) and are fixed by the pinned platform. For advisories the IDF version is the newest tag the library commit contains (5.5.6, checked over the GitHub API). `lib_archive = yes` is set explicitly: pioarduino defaults to linking library objects instead of archives, which linked the unused Arduino `SPIFFS` and `LittleFS` objects with their constructors (+10.7 KB flash). Result: 0 warnings in `src/`/`include/`, flash 1 553 065 B (+26.0 KB, mostly WiFi library, newlib/GCC 14.4, mDNS, lwIP), static RAM 78 992 B (+384 B). Costs: a second platform source with its own release cadence (the opposite of E-511, without `custom_sdkconfig`), and the platform installs its Python dependencies (minimum versions, not pinned) and pioarduino's fork of PlatformIO Core into PlatformIO's `penv`; its first install refuses to run under Git Bash/MSYS. Back to the PlatformIO registry platform once it offers Arduino 3.3.13 or later (task REL-03); the fallback in an emergency is `espressif32@8.0.0` (Arduino 3.3.12, with the advisory). Unity 2.7.0 follows once it is in the registry (task REL-02). The job "Release dry run" is a required check for `main`.

#### E-805 Build information
*Decided.* `tools/build_info.py` (PlatformIO `extra_scripts = pre:`) determines branch, commit, dirty mark and build time and passes them only to `src/utils/build_info.cpp` (`env.AddBuildMiddleware`), so each build recompiles just this file. `__DATE__` / `__TIME__` are unsuitable: the file would not be recompiled and the date would go stale. Shown on the CONFIG info card and in the boot log line; not part of the host tests. Since E-806 the stamp comes from the commit.

#### E-806 Build stamps from the commit, not from the build
*(Step AF25, audit OPS-13.)* The image carried the local build time without a zone and, for a release candidate, `<version> | HEAD` (no rc suffix, detached HEAD), and the release archives held the build machine's times and owners, so no independent rebuild could be compared with the attested assets. The stamp now comes from the commit: the time is `SOURCE_DATE_EPOCH` or the commit time of HEAD in UTC, the branch at a detached HEAD is the tag, and a build of a tag `v<APP_VERSION>[-suffix]` shows the tag version (e.g. `<version>-rc1`). Only a build with uncommitted changes (`+`) uses the current time. The license and LGPL archives are sorted, with the commit time, owner 0 and a gzip header without name and time. Two builds of the same commit gave identical `build_info` text and identical archives. `*IDN?` and the REST version stay `APP_VERSION`.

#### E-807 CI triggers, pins and gates
*(Audit priority 3, decided by the user.)* CI, fuzzing and CodeQL run on push only for `main`, `dev` and `agent/**` (pull requests to `main` always), tags only in the release workflow, no cancelling on `main` (AF21); the framework is pinned exactly, the release builds without cache, and a dry run of the release path runs in pull requests to `main` (AF22, AF23); warnings in `src/` fail CI, no additional lint (AF24); stamps from tag and commit time in UTC, rc suffix on the info card (AF25, E-806); a weekly advisory check, a finding = red run and issue, also before tags (AF26); tool hygiene with a CI guard against a committed `include/test_wifi_local.h` (AF27); repository settings set over the API (AF28, E-811).

#### E-808 Every `v*` tag deploys the web flasher
*(Decided by the user.)* Every pushed `v*` tag deploys the web flasher after its release, release candidates (`-rc`) included.

#### E-809 Tag ruleset and immutable releases
*(Steps AF7, AF28, decided by the user.)* The tag ruleset `release-tags` (`refs/tags/v*`: no deletion, no move, no force push, without exceptions) and immutable releases are switched on. The tag of the first release candidate (without a release) stays and is only noted in the history (step G2).

#### E-810 No manual deploy of the web flasher
*(Step AF7, audit LIC-6, decided by the user.)* Only what a `v*` tag triggers goes public. A manual run builds and packs the site as an artifact for testing; the former input `deploy` is gone.

#### E-811 Repository settings
*(Step AF28, audit OPS-7, OPS-12, OPS-16, decided by the user.)* Set over the API: wiki off; actions only from GitHub itself (`allowed_actions: selected`, `github_owned_allowed`, no verified creators, no patterns) with SHA pinning required (`sha_pinning_required`); pull requests only as merge commit (repository setting and ruleset `main` `allowed_merge_methods: [merge]`); `main` changes only by pull request with green required checks.

---

## 9. Documentation and licensing

### 9.1 Requirements

- **A-901** Licensing of distributed binaries (step G11): the project is GPL-3.0-or-later, "Copyright (C) 2026 DM8SB". Every release carries the license and third-party notices with the license texts (`…-licenses.tar.gz`) and the complete source of the LGPL libraries linked into the firmware with the board variant, the framework build scripts, the license texts and a README naming the upstream versions (`…-lgpl-sources.tar.gz`); `THIRD_PARTY_NOTICES.md` lists every component of the firmware, the flasher site, tests and tools with version, SPDX license, copyright holder and source, is updated with every dependency change in `platformio.ini` or `flasher/esp-web-tools.json` and is compared with the linker map and Espressif's list of firmware components when the platform changes; the release build fails if a notice file is missing. The firmware carries the additional permission under GPLv3 section 7 for Espressif's object-code-only libraries (`LICENSE-EXCEPTION.md`, E-906); the source and license links of the web UI and the flasher page point to the tag or commit of the build (E-907). Own source files carry `SPDX-FileCopyrightText: 2026 DM8SB` and `SPDX-License-Identifier: GPL-3.0-or-later`; `REUSE.toml` covers files without comments (E-908). The flasher page links the GPL text, the notices, the LGPL sources, the license files of ESP Web Tools and its bundled packages and the source of the GPL-2.0-or-later flasher stubs it serves; it names the person behind it, states the hobby character and the absence of warranty, and has a privacy note. The web UI shows the license line in its footer; the device display does not.

### 9.2 Decisions

#### E-901 Documentation roles and planning files
*Decided.* Specification = requirements and decisions; behavior only in `USER_MANUAL_DE.md`; `USER_MANUAL_EN.md` is its translation with the same structure and tables, updated in the same commit. Reason: the former specification (1401 lines) and both manuals described the same behavior and had drifted apart. One planning file (`ARBEITSPLAN.md`: rules, status, open findings, open tasks) instead of separate to-do, audit and concept files; all decisions live in this specification. The measurement plan (`MESSPLAN.md`) is ordered by priority: tier 1 mandatory, tier 1W mandatory with the WEB-HMI, tier 2 for decisions, tier 3 optional. Quality, operation and the WEB-HMI came before the measurement campaign; tier 1 (and if possible tier 2) runs as soon as a counter and an oscilloscope are available, and that run is the reference R0 (a separate R0 run on an old version is not needed).

#### E-902 Remote control documentation
*(WEB-HMI concept.)* SCPI reference, REST API, a Python example, pointers to PyVISA, LabVIEW, NI-MAX and MATLAB, security notes (manual sections 15 and 16, `docs/REMOTE_API.md`).

#### E-903 Manual, hardware page and remote API as pages of the flasher site, rendered at the tag
*(Step G9, decided by the user.)* The user manual (DE, EN), `docs/hardware.md` and `docs/REMOTE_API.md` are published on GitHub Pages next to the web flasher (`manual/de.html`, `manual/en.html`, `hardware.html`, `remote-api.html`, images used by them under `img/`, one stylesheet), and the flasher page links them. They are rendered by the release workflow from the Markdown of the tagged commit and deployed with the flasher, so the pages always describe the firmware the flasher installs; there is no second branch or copy to keep in step. Renderer: markdown-it-py 4.2.0 (CommonMark with GFM tables, MIT, only dependency mdurl), pinned with hashes in `tools/requirements-ci.txt`; Python-Markdown was not taken because it parses lists differently from GitHub (no list without a blank line before it, nesting only with four spaces), and the manuals would render wrongly without changes. Heading anchors follow GitHub's slug rules, so the anchors written for GitHub work on the site unchanged; links to the published documents point to their pages, links to other repository files to GitHub at the tag (as E-907). The pages carry no script, the CSP of the flasher without `script-src` and without `'unsafe-inline'`, and no resource from another origin. The site check fails on a missing page, a broken link, anchor or image, a style or event attribute or an unexpected tag; it runs in the release dry run of every pull request to `main` and, docs only, in the tool checks of CI. Measured at the first deploy: four pages 410 KB, 20 images 628 KB, site 5.9 MB → 7.0 MB.

#### E-904 License, copyright and commit identity
*(Decided by the user.)* License GPL-3.0-or-later with the copyright line "Copyright (C) 2026 DM8SB"; a private project with the single commit identity `DM8SB <dm8sb@gmx.de>`, the history rewritten to it; before the first release candidate of the 2.x line the git history was replaced by a new root commit, old tags and releases were deleted (rulesets switched off and on again for it). ESP Web Tools are self-hosted on GitHub Pages (no CDN, E-905); no imprint, but name, hobby project and the warranty disclaimer on the flasher page; the license line only in the web UI, not on the display (A-901).

#### E-905 ESP Web Tools self-hosted; no imprint, hobby statement
*(Step G11, decided by the user.)* The flasher site serves ESP Web Tools 10.4.0 itself instead of loading it from jsDelivr: the release workflow downloads the 26 modules pinned with SHA-384 in `flasher/esp-web-tools.json`, checks every hash and publishes them under `esp-web-tools/<version>/`, so a visitor's browser contacts no third party (privacy) and the CSP allows scripts only from `'self'` (no import map needed any more; styles keep `'unsafe-inline'` because ESP Web Tools sets style attributes). Self-hosting makes the site a distributor: the license files of the bundled packages and, for the GPL-2.0-or-later flasher stubs (identified as esptool-legacy-flasher-stub v1.3.0, v1.6.0, v1.10.0, v1.11.1 by comparing their code), the complete source archives are served next to them (pinned with SHA-256). Measured: site 5.6 MB, of which LGPL sources 2.2 MB and stub sources 1.1 MB. No full imprint: the page is a purely private, non-commercial hobby project; it names Sebastian Brandes (DM8SB), disclaims warranty and liability along GPL sections 15/16 and says that GitHub Pages processes technically necessary data (IP address) while the page itself sets no cookies, tracks nothing and requests nothing from third parties.

#### E-906 GPLv3 section 7 permission for Espressif's object-code-only libraries
*(Step AF30, audit LIC-1, wording confirmed by the user, no legal advice.)* The GPL-3.0-or-later firmware is linked with libraries Espressif ships only as object code (Wi-Fi, PHY/RF, coexistence, SmartConfig, ESP-NOW, mesh, RTC, Bluetooth controller); their source is not published, so whoever conveys the binary or a fork could not supply the complete Corresponding Source. `LICENSE-EXCEPTION.md` therefore grants an additional permission under GPLv3 section 7 to convey the work combined with these libraries, in the wording of the FSF template for GPL-incompatible libraries (GPL FAQ "GPLIncompatibleLibs") without the optional sentence that would require the libraries' source; all other parts, including the LGPL libraries, keep their source obligation. The SPDX identifier stays `GPL-3.0-or-later` (SPDX has no identifier for a project-specific exception); README, `THIRD_PARTY_NOTICES.md`, the flasher page and the release notices carry the permission, and contributions are accepted only under it. Alternatives rejected: doing nothing (tolerated in practice, but leaves third parties in breach) and relicensing (contradicts the GPL choice). `libbtdm_app` stays: Arduino's `initArduino()` always calls `btMemRelease()`, whose `esp_bt_mem_release()` in ESP-IDF `bt.c` pulls in 71 members of the library, of which about 0.5 KB remain in the image after `--gc-sections` (394 B IRAM code, 21 B rodata, 53 B data/bss; `bt.c` and the forced `hli_vectors` add 0.7 KB flash and 4.9 KB IRAM/DRAM). Dropping it would need stub definitions of `esp_bt_mem_release`/`esp_bt_controller_mem_release` and removing `-u ld_include_hli_vectors_bt` from the framework's linker flags; that changes how the BT memory regions reach the heap at boot and can only be verified on the device, for at most about 6 KB, so it is not done without an own ESP-IDF build.

#### E-907 Source and license links point to the tag of the build
*(Step AF30, audit LIC-6.)* The web UI footer and the flasher page linked the repository and `main`, which moves after a release. The footer now links `tree/<tag>` and `blob/<tag>/THIRD_PARTY_NOTICES.md`: `tools/embed_www.py` replaces `{{SRC_REF}}` at build time with the tag pushed in the release workflow, a `v*` tag on HEAD, or else `v<APP_VERSION>` (the release a development build leads to). The commit is not used there because the page is readable without login (S-26); the tag changes rarely, so `www_assets.h` is not rewritten on every commit. The flasher page links the release and `tree/v<version>` of a tag build and `tree/<commit>` of a manual build (no release exists); `flasher_site.py check` fails without such a link. Public deploys only from tags (E-810) belong to `release.yml` (step AF7).

#### E-908 SPDX headers in all own files
*(Step AF31, audit LIC-8, decided by the user.)* SPDX headers (`SPDX-FileCopyrightText: 2026 DM8SB`, `SPDX-License-Identifier: GPL-3.0-or-later`) in all own files, the rest through `REUSE.toml`; new files get them too. `LICENSES/` is not rebuilt REUSE-compliant for now, no `reuse lint` in CI.
