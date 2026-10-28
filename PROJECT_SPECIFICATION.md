# CYD HF Generator – Complete Project Specification

## 1. Project Goal

The goal of this project is to implement a maintainable, modular, touchscreen-controlled **3-channel HF square-wave generator** based on:

- **ESP32-2432S028R / Sunton CYD / Cheap Yellow Display**
- **ILI9341 320×240 TFT display** (original CYD) or **ST7789** (CYD2USB, additional USB-C port)
- **XPT2046 resistive touch controller**
- **SI5351 clock generator with 25 MHz crystal**
- **PlatformIO + Arduino-ESP32**
- **LVGL GUI**

The device shall generate digital HF square-wave signals with 50 % duty cycle in the range:

```text
1 MHz to 100 MHz
```

The device provides:

| Channel | SI5351 Output | Function |
|---|---|---|
| CH1 | CLK0 | Independent frequency output |
| CH2 | CLK1 | Independent frequency output |
| CH3 | CLK2 | IQ follower of CH1 or CH2 |

CH3 always follows the selected master frequency and can apply a phase offset of:

```text
0° / 90° / 180° / 270°
```

CH3 IQ mode is available for master frequencies from 4.730 MHz to 100 MHz; all four phases are valid in that range.

If a requested frequency or phase cannot be represented safely by the SI5351 configuration, the firmware shall reject the value or keep the output safely off and operate safely.

Firmware version described: **1.2.0**. Changes against 1.0.0 and the approved deviations from the original specification are listed in sections 40 and 41.

---

## 2. Current Confirmed Hardware Configuration

This project uses the following confirmed wiring.

The original specification listed SI5351 I2C as `SDA=27`, `SCL=22`. During testing on the actual hardware, the working wiring was verified by I2C scan as `SDA=22`, `SCL=27`, SI5351 address `0x60`.

Both wirings are supported without a code change: the firmware detects which of the two CN1 pins is SDA (section 42). `include/pins.h` only names the two candidates:

```cpp
static constexpr int PIN_I2C_A = 22;   // tried first as SDA (verified wiring)
static constexpr int PIN_I2C_B = 27;
```

---

## 3. Hardware Overview

### 3.1 Controller Board

Board:

```text
ESP32-2432S028R
Cheap Yellow Display / Sunton CYD
```

Target framework:

```text
PlatformIO + Arduino-ESP32
```

No WiFi, no OTA and no external buttons are used.

---

## 4. Display Hardware

### 4.1 Display Type

```text
ILI9341 TFT (original CYD) or ST7789 TFT (CYD2USB)
Resolution: 320 × 240
Orientation: Landscape
SPI bus: HSPI
```

Two boards are sold as ESP32-2432S028R. They differ only in the display controller:

| Board | How to recognize it | Display controller |
|---|---|---|
| Original CYD | micro-USB port only | ILI9341 |
| CYD2USB | additional USB-C port | ST7789 |

Pinout, touch, backlight and SI5351 wiring are identical. One firmware serves both; it detects the controller at boot (section 43).

### 4.2 Display Pinout

| Function | ESP32 GPIO |
|---|---:|
| TFT MOSI | GPIO13 |
| TFT MISO | GPIO12 |
| TFT SCK | GPIO14 |
| TFT CS | GPIO15 |
| TFT DC | GPIO2 |
| TFT RST | -1 / not used / connected to EN |

### 4.3 Display Library

```text
TFT_eSPI
```

The display is configured through PlatformIO build flags, not by editing the library files. TFT_eSPI selects the controller at compile time; it is built for the ILI9341 (`-DILI9341_DRIVER=1`, SPI mode 0). `DisplayPanel` (`drivers/display_panel.*`) detects the controller at boot and, on the ST7789, replaces the init sequence and the MADCTL values (section 43). TFT_eSPI is pinned to 2.5.43 because the ST7789 init sequence is copied from it.

---

## 5. Touch Hardware

### 5.1 Touch Controller

```text
XPT2046
SPI bus: VSPI
Library: PaulStoffregen/XPT2046_Touchscreen v1.4
```

### 5.2 Touch Pinout

| Function | ESP32 GPIO |
|---|---:|
| Touch MOSI | GPIO32 |
| Touch MISO | GPIO39 |
| Touch SCK | GPIO25 |
| Touch CS | GPIO33 |
| Touch IRQ | GPIO36 |

### 5.3 Touch Calibration Policy

Runtime touch calibration is currently disabled.

The firmware always uses fixed default calibration values (`TOUCH_CAL_DEFAULT` in `include/constants.h`):

```text
x_min = 300
x_max = 3800
y_min = 300
y_max = 3800
```

The touch calibration button and runtime calibration routine are intentionally disabled.

Touch input still includes:

- basic coordinate mapping
- jitter filtering
- debounce handling
- LVGL pointer input integration

---

## 6. SI5351 Clock Generator

### 6.1 Clock Generator

```text
SI5351
Crystal: 25 MHz
I2C address: 0x60
```

### 6.2 Confirmed I2C Pinout

| SI5351 | ESP32 GPIO |
|---|---:|
| SDA / SCL | GPIO22 / GPIO27 (either order, detected) |
| VCC | 3.3 V |
| GND | GND |

I2C clock:

```text
100 kHz during detection, 400 kHz after a successful init
```

Expected serial output (straight / swapped wiring):

```text
[APP] SI5351 at 0x60, SDA=22 SCL=27
[APP] SI5351 at 0x60, SDA=27 SCL=22 (swapped)
```

A full bus scan runs at startup only with `LOG_DEBUG 1`.

### 6.3 Important SI5351 Hardware Notes

Many SI5351 breakout boards have an `OE` pin.

Often this pin is active-low:

```text
OE = GND  -> outputs enabled
OE = HIGH -> outputs disabled
```

If no output signal is present despite correct I2C communication, check the OE pin.

---

## 7. Backlight

### 7.1 Backlight Pin

```text
GPIO21
```

### 7.2 Backlight Rules

GPIO21 is reserved exclusively for backlight PWM.

It must not be used for:

- I2C
- buttons
- LEDs
- any other peripheral

### 7.3 Backlight Control

Backlight is controlled by LEDC PWM.

User range:

```text
5 % to 100 %
```

The minimum of 5 % (`BACKLIGHT_MIN_PERCENT`) prevents a dark, unusable display. A stored value below 5 % is replaced by the default 80 % when settings are loaded.

Internal PWM range:

```text
0 to 255
```

---

## 8. RGB LED

The board RGB LED is active-low.

| LED Color | ESP32 GPIO |
|---|---:|
| Red | GPIO4 |
| Green | GPIO16 |
| Blue | GPIO17 |

### 8.1 LED Behavior

The RGB LED indicates the active channel page:

| Page | LED Color |
|---|---|
| CH1 | Red |
| CH2 | Green |
| CH3 | Blue |
| Main / Config / Numpad | Off |

---

## 9. Functional Requirements

### 9.1 Generator Type

The device is a digital HF square-wave generator.

Signal properties:

```text
Waveform: square wave
Duty cycle: 50 %
Frequency range: 1 MHz to 100 MHz
Input resolution: 1 Hz
```

### 9.2 Channel Behavior

#### CH1

- Free frequency
- ON/OFF switchable
- Output: SI5351 CLK0

#### CH2

- Free frequency
- ON/OFF switchable
- Output: SI5351 CLK1

#### CH3

- IQ follower
- Output: SI5351 CLK2
- Master selectable:
  - CH1
  - CH2
- Same frequency as selected master
- Phase selectable:
  - 0°
  - 90°
  - 180°
  - 270°
- IQ range: master frequency 4.730 MHz to 100 MHz
- CH3 output is active only if:
  - CH3 is ON
  - selected master channel is ON
  - master frequency is at least 4.730 MHz
- The CH3 ON/OFF setting is the user's request. If the conditions are not met, CLK2 stays off but the request is kept; CH3 starts automatically as soon as the conditions are met.

### 9.3 OFF State

When a channel is OFF, the corresponding SI5351 output is disabled.

The GUI displays:

```text
ON
OFF
```

The GUI does not display `High-Z`.

---

## 10. SI5351 Configuration Rules

### 10.1 Driver

The firmware uses its own register-level driver (`SI5351Manager`, Skyworks AN619); no external SI5351 library is used.

- **Correction**: the crystal is modeled as 25 MHz × (1 + correction × 10⁻⁸) and all dividers are computed against it with 64-bit integer math. This applies the correction exactly at every frequency (the former library truncated the frequency to whole MHz).
- **Dividers**: fractions are the best rational approximation with a 20-bit denominator (continued fractions).
- **Standalone outputs**: the PLL stays at a fixed value (900 MHz or 887.5 MHz) and only the MultiSynth divider changes. The second PLL value is used only where the first cannot reach ≤ 0.25 Hz (near 900 MHz / n); the current PLL is kept while it is good enough. Error ≤ 0.5 Hz up to ~110 MHz. Above that, an even integer MultiSynth divider (6 or 4) and a fractional PLL are used. Below 1 MHz the R divider is used.
- **Register cache**: only changed registers are written. A PLL is reset (register 177: bit 5 = PLL A, bit 7 = PLL B) only when its parameters changed, so tuning does not interrupt the output.
- **Unused MultiSynths** are powered down (CLK2 while CH3 is not in IQ mode).
- **Supervision**: every 2 s the firmware reads back the output enable and CLK control registers. A failed transfer or unexpected content (e.g. after a power interruption of the SI5351) marks the device as lost; the firmware then tries to re-initialize it every 2 s and restores all outputs.
- Crystal load capacitance 10 pF (register 183 = 0xD2).

### 10.2 Outputs

| Channel | SI5351 CLK |
|---|---|
| CH1 | CLK0 |
| CH2 | CLK1 |
| CH3 | CLK2 |

### 10.3 Drive Strength

Drive strength is global for all channels.

Available values:

```text
2 mA
4 mA
6 mA
8 mA
```

The value is configured on the CONFIG page under:

```text
Drive all
```

CH3 has no independent drive setting.

### 10.4 Frequency Switching Sequence

Standalone outputs are retuned without disabling them: only the changed MultiSynth registers are written. The PLL is written and reset only when its parameters change (rare, see 10.1). For an IQ pair, both outputs are disabled, reconfigured, the PLL is reset to align the phases, and the outputs are re-enabled if ON; a pure drive strength or inversion change is written without reset.

Switching artifacts are allowed.

A PLL reset affects only the PLL of the channel being changed, so a frequency change on one channel does not interrupt the channel running on the other PLL:

| Channel | PLL |
|---|---|
| CH1 | PLL A |
| CH2 | PLL B |
| CH3 | PLL of its master |

### 10.5 CH3 IQ Mode

CH3 must share the same PLL as its selected master.

When CH3 is enabled and the master frequency is at least 4.730 MHz:

- master and CH3 are configured as an IQ pair: integer MultiSynth divider d = min(127, ⌊900 MHz / f⌋) in fractional mode, PLL = d × f
- same frequency
- phase applied to CH3 (see 10.6)
- the shared PLL is reset after writing the phase offsets, which aligns both outputs

When CH3 is disabled, or enabled but the master is below 4.730 MHz:

- CLK2 is disabled and its MultiSynth powered down
- CH1 and CH2 are configured as standalone outputs

Lower limit: the divider is at most 127 (7-bit phase register). The VCO stays within its 600 MHz minimum only from 600 MHz / 127 ≈ 4.725 MHz upwards; the firmware uses `IQ_MIN_HZ = 4 730 000 Hz`. Upper limit: the divider must be ≥ 9 in fractional mode, so `IQ_MAX_HZ = 100 MHz`. Accuracy ≤ 1 Hz (worst case next to multiples of the crystal frequency, e.g. 75 MHz).

### 10.6 Phase Validation

The SI5351 phase offset register is 7-bit. One unit is a quarter VCO period, so 90° equals the MultiSynth divider, which is at most 127 in the IQ range.

The phases are realized as:

| Phase | Phase register | CLK2 inverted (CLK_INV) |
|---|---|---|
| 0° | 0 | no |
| 90° | divider | no |
| 180° | 0 | yes |
| 270° | divider | yes |

Therefore all four phases are valid in the whole IQ range of 4.730–100 MHz.

If the master frequency is below 4.730 MHz:

- CLK2 is kept disabled
- the CH3 ON request is kept
- the GUI shows the state as `WAIT` / "Waiting: CHx below 4.730 MHz"
- no message box is shown

---

## 11. GUI Specification

### 11.1 GUI Framework

```text
LVGL 8.3.x
```

Display resolution:

```text
320 × 240 landscape
```

### 11.2 General Layout Style

The GUI uses:

- dark background
- colored cards
- colored channel buttons
- title/header bars on subpages
- ON/OFF switch with ON/OFF label in the title bar on channel pages, `MAIN` button at the right
- a common layout grid (`src/gui/gui_layout.h`: margin 8 px, header 38 px, card padding 6 px) so that no control is clipped, overlaps or makes a page scrollable
- touch targets of at least about 36 px height for frequently used controls
- dark text on yellow buttons for contrast

### 11.3 Color Scheme

| Function | Color |
|---|---|
| CH1 | Red |
| CH2 | Green |
| CH3 | Blue |
| CONFIG | Yellow |
| Back/Main buttons | Gray |

### 11.4 Frequency Display Format

Frequencies are displayed in MHz with grouped decimal digits:

```text
1.000 000 MHz
5.000 000 MHz
10.700 000 MHz
100.000 000 MHz
```

---

## 12. GUI Pages

The GUI contains these pages:

```text
MAIN
CH1
CH2
CH3
CONFIG
NUMPAD
```

---

## 13. MAIN Page

### 13.1 Purpose

The main page provides a quick overview and navigation.

### 13.2 Main Page Content

The main page displays one card per channel:

- CH1 frequency and state
- CH2 frequency and state
- CH3 frequency, master, phase and state (detail line e.g. `CH1 / 90 deg`, plus the reason when waiting)
- colored navigation buttons:
  - CH1 red
  - CH2 green
  - CH3 blue
  - CONFIG yellow

Each card:

- is tappable and opens the channel page
- has its own ON/OFF switch that toggles the output directly

State text below the channel name:

| State | Meaning |
|---|---|
| ON | output active |
| OFF | output off |
| WAIT | CH3 requested but not active (master off or below 4.730 MHz) |
| NO HW | SI5351 not found |

The project title header is intentionally removed to maximize available status space.

---

## 14. CH1 Page

### 14.1 Purpose

Control CH1 frequency and output state.

### 14.2 Elements

- Header with CH1 title
- ON/OFF switch in title row
- Frequency display
- Info line: `Step <step>  |  Deviation <±x.x> Hz`
- The digit changed by `+`/`-` is highlighted in the channel color
- Step preset buttons: 1, 10, 100, 1k, 10k, 100k, 1M (Hz); the selected step is highlighted
- `-` button (left)
- `+` button (right)
- `NUMPAD` button
- `MAIN` button

### 14.3 Behavior

- `+` increases CH1 frequency by delta step
- `-` decreases CH1 frequency by delta step
- holding `+` / `-` repeats the step (first repeat after 0.4 s, then every 0.1 s)
- the frequency is clamped to 1–100 MHz
- `NUMPAD` opens numeric frequency entry
- tapping a digit of the frequency display selects it as step (digits above 1 MHz select 1 MHz); tapping the unit opens the numpad
- ON/OFF switch enables/disables CLK0
- the requested frequency is stored exactly (1 Hz); "Deviation" is the calculated difference of the synthesized SI5351 frequency from the requested one, assuming the correction is right

---

## 15. CH2 Page

Same as CH1, but for:

```text
CH2 / CLK1
```

---

## 16. CH3 Page

### 16.1 Purpose

Configure CH3 as IQ follower.

### 16.2 Elements

- Header with CH3 IQ title
- ON/OFF switch in title row
- read-only frequency display
- status line (`Off | follows CHx`, `IQ active, follows CHx`, `Waiting: master CHx is off`, `Waiting: CHx below 4.730 MHz`, `SI5351 not found`)
- master selection as buttons:
  - CH1
  - CH2
- phase selection as buttons (degrees):
  - 0
  - 90
  - 180
  - 270
- note line with the IQ range and the drive-strength hint


### 16.3 Behavior

CH3 follows its master frequency.

If CH3 is ON but its master is OFF, or the master is below 4.730 MHz, CH3 output remains OFF. The switch stays ON, the state is displayed as WAIT with the reason, and CH3 starts automatically when the condition is resolved.

---

## 17. CONFIG Page

### 17.1 Purpose

The CONFIG page contains global device settings.

### 17.2 Elements

- Backlight slider, 5–100 %, with value label
- Step controls over the presets 1 Hz … 1 MHz:
  - x10 (next larger preset)
  - /10 (next smaller preset)
- Frequency correction:
  - value shown in ppm (internal unit 0.01 ppm, see below)
  - step button cycling 0.01 / 0.1 / 1 ppm
  - `-` / `+` buttons with hold-to-repeat
  - range ±50 ppm
- Global drive-strength dropdown `Drive all`:
  - 2 mA
  - 4 mA
  - 6 mA
  - 8 mA
- Autosave indicator: `Saving...` / `Saved` / `Save failed` / `Not saved` (NVS unavailable)

Correction unit: `freq_correction` is stored in 0.01 ppm (= Hz deviation at 100 MHz). Positive means the crystal, and thus the uncorrected output, is too high. Calibration: measure at 10 MHz, deviation in Hz / 10 = ppm (e.g. +97 Hz → +9.70 ppm).

A correction change reconfigures the outputs but does not re-initialize the SI5351.

### 17.3 Touch Calibration

Touch calibration UI is disabled.

There is no touch-calibration button.

---

## 18. NUMPAD Page

### 18.1 Purpose

Numeric frequency input for CH1 and CH2.

### 18.2 Elements

Layout (4×4 grid, about 44 px per row):

```text
 7   8   9   MHz
 4   5   6   kHz
 1   2   3   Hz
 .   0   ⌫   C
```

- numeric buttons `0` to `9`
- decimal point `.` (only one allowed)
- backspace `⌫`
- `C` (clear)
- unit buttons (highlighted in channel color):
  - `Hz`
  - `kHz`
  - `MHz`
- back button `◀` at the top left: returns to the channel page without change
- input field showing the current frequency as placeholder

There is no separate OK button.

### 18.3 Behavior

Pressing a unit button confirms the input immediately and returns to the channel page.

Invalid input (empty, unparsable, or outside 1–100 MHz) is rejected and not clamped: the field is cleared, the border turns red and the hint `1 - 100 MHz` is shown.

Examples:

```text
10 + MHz      -> 10.000 000 MHz
10.7 + MHz    -> 10.700 000 MHz
1000 + kHz    -> 1.000 000 MHz
5000000 + Hz  -> 5.000 000 MHz
```

Maximum input length:

```text
9 characters
```

---

## 19. Settings and Persistence

### 19.1 Storage Backend

The firmware uses:

```text
Preferences / NVS
Namespace: hfgen
```

### 19.2 Stored Values

Stored values include:

- CH1 frequency
- CH2 frequency
- CH3 master selection
- CH3 phase
- backlight percentage
- delta step
- frequency correction
- global drive strength

The ON/OFF state of the outputs is not stored; all outputs start off (approved 2026-10-27). Keys `ch1_en` / `ch2_en` / `ch3_en` from 1.1.0 are ignored. Touch calibration is not stored. Backlight values below 5 % are replaced by 80 %, the step is snapped to the nearest preset and the correction is limited to ±50 ppm when loading.


### 19.3 Autosave

Autosave uses a 5 second debounce.

Process:

```text
1. Setting changes
2. Dirty flag set
3. If no further change for 5 seconds
4. Save all persistent settings to NVS
5. Print Serial log
```

If NVS cannot be opened at startup, a dialog is shown and nothing is saved (CONFIG: `Not saved`). If a save fails, a dialog is shown (CONFIG: `Save failed`).

Expected log:

```text
[SETTINGS] Saved to NVS
```

---

## 20. Error Handling

### 20.1 Serial Logging

Serial logging is mandatory.

The firmware logs:

- boot status
- I2C initialization
- I2C scan results (only with `LOG_DEBUG 1`)
- SI5351 connection state changes with the detected SDA/SCL assignment
- SI5351 init
- frequency changes
- correction changes
- settings save events
- touch initialization
- SI5351 setup errors

Verbose SI5351 logs (output mask, phase register, per-CLK setup) are only compiled in with `LOG_DEBUG 1` in `include/config.h`, because blocking serial output slows down the GUI.

### 20.2 GUI Error Reporting

On important errors, an LVGL message box is shown.

Examples:

- SI5351 not detected (shown once at boot; afterwards the state is shown as NO HW / "SI5351 not found")
- I2C line stays low, or the device at 0x60 is not an SI5351 (once per state change, also at runtime)
- SI5351 frequency or IQ setup error

Rules:

- `OK` closes the message box
- connection dialogs (`SI5351 not found`, `I2C line stays low`, `not an SI5351`, `Connection lost`) close by themselves when the SI5351 is connected again; output error dialogs stay
- at most one message box is open; a new message replaces the old one
- an identical message is not shown again while it is open
- without SI5351 the firmware does not attempt hardware configuration, so no repeated errors are generated

### 20.3 Safe Operation

On SI5351-related errors:

- do not crash
- log the error
- show message if appropriate
- keep or return outputs to a safe state

---

## 21. Software Architecture

### 21.1 Project Type

```text
PlatformIO project
Framework: Arduino-ESP32
GUI: LVGL
```

### 21.2 Architecture Style

Layered modular embedded MVC-style architecture.

```text
+-----------------------+
| GUI                   |
| LVGL pages/widgets    |
+-----------------------+
| App Controller        |
| Business logic/state  |
+-----------------------+
| Services              |
| Settings/Storage      |
+-----------------------+
| Drivers               |
| SI5351/Touch/PWM      |
+-----------------------+
| HAL                   |
| I2C abstraction       |
+-----------------------+
| Hardware              |
+-----------------------+
```

---

## 22. Directory Structure

project structure:

```text
cyd_hf_generator/
|
├── platformio.ini
├── README.md
├── USER_MANUAL_DE.md
├── USER_MANUAL_EN.md
├── PROJECT_SPECIFICATION.md
├── REFACTOR_NOTES.md
|
├── include/
|   ├── config.h
|   ├── constants.h
|   ├── lv_conf.h
|   ├── pins.h
|   └── types.h
|
├── src/
|   ├── main.cpp
|   |
|   ├── core/
|   |   ├── app_controller.cpp
|   |   └── app_controller.h
|   |
|   ├── drivers/
|   |   ├── backlight.cpp
|   |   ├── backlight.h
|   |   ├── si5351_manager.cpp
|   |   ├── si5351_manager.h
|   |   ├── status_led.cpp
|   |   ├── status_led.h
|   |   ├── touch.cpp
|   |   └── touch.h
|   |
|   ├── gui/
|   |   ├── gui.cpp
|   |   ├── gui.h
|   |   ├── gui_layout.h
|   |   └── pages/
|   |       ├── page_main.cpp
|   |       ├── page_channel.cpp
|   |       ├── page_ch3.cpp
|   |       ├── page_config.cpp
|   |       └── page_numpad.cpp
|   |
|   ├── hal/
|   |   ├── i2c_bus.cpp
|   |   └── i2c_bus.h
|   |
|   ├── services/
|   |   ├── settings.cpp
|   |   ├── settings.h
|   |   ├── storage.cpp
|   |   └── storage.h
|   |
|   └── utils/
|       ├── frequency_utils.cpp
|       ├── frequency_utils.h
|       ├── logger.cpp
|       └── logger.h
|
└── lv_conf.h
```

---

## 23. Main Initialization Order

The firmware initialization order is:

```text
1. Serial / Logger
2. I2C bus initialization (release a line held low, default assignment at 100 kHz; scan only with LOG_DEBUG)
3. Backlight PWM (backlight off)
4. Settings load from NVS
5. SI5351 detection with both SDA/SCL assignments, identity check and init (`app.connectHardware()`)
6. Touch initialization
7. Display: boot gesture check, controller detection, init (ILI9341 sequence, then ST7789 sequence if needed), landscape, black
8. LVGL and GUI initialization, boot dialogs (SI5351 missing, NVS unavailable)
9. First frame rendered, then backlight on (the uninitialized panel would show noise)
10. Program stored frequencies; all outputs off
```

The main loop:

```cpp
void loop() {
    lv_timer_handler();
    app.update();
    GUI.update();
    delay(5);
}
```

---

## 24. Core State Model

### 24.1 Channel

```cpp
struct Channel {
    uint64_t frequency;   // requested frequency in Hz (exact, 1 Hz resolution)
    bool enabled;
    uint8_t drive_strength;
};
```

### 24.2 Channel3

```cpp
struct Channel3 {
    bool enabled;       // requested by the user; see Ch3Status
    uint8_t master;     // 1 or 2
    uint16_t phase;     // 0/90/180/270
    uint8_t drive_strength;
};
```

Note: CH3 contains `drive_strength` for internal state compatibility, but the user-facing drive-strength setting is global.

### 24.3 SystemSettings

```cpp
struct SystemSettings {
    uint32_t delta_step;       // one of the presets 1 Hz ... 1 MHz
    int32_t freq_correction;   // 0.01 ppm, +-5000
    uint8_t backlight;         // 5-100 %
    uint8_t default_drive_strength;
};
```

### 24.4 Ch3Status

Derived (not stored) CH3 output state used by the GUI:

```cpp
enum class Ch3Status : uint8_t { Off, Active, MasterOff, FreqTooLow, NoDevice };
```

---

## 25. AppController Responsibilities

`AppController` is the central state owner.

Responsibilities:

- hold current channel state
- validate channel numbers and settings
- clamp frequency requests to 1–100 MHz (stored exactly, no rounding)
- connect the SI5351 at boot and every 2 s while it is missing, trying both SDA/SCL assignments (`connect_()`, `LinkState`)
- coordinate SI5351 reconfiguration (skipped without SI5351)
- coordinate CH3 follow logic and derive `Ch3Status`
- apply global drive strength
- update backlight
- mark settings dirty for autosave
- report errors to GUI

Only the AppController should call SI5351Manager directly from the business logic layer.

---

## 26. SI5351Manager Responsibilities

`SI5351Manager` abstracts low-level SI5351 operations.

Responsibilities:

- initialize SI5351
- probe I2C address 0x60 and check the identity (registers 0 and 183, read-back of register 3) before any further write
- never touch `Wire` while the bus is not open (`I2CBus::isOpen()`)
- configure standalone frequency outputs
- configure IQ pair for CH3 (phase register + CLK_INV)
- compute PLL / MultiSynth / R divider values (exact correction, best rational approximation)
- cache register contents, write only changes, reset a PLL only when it changed
- power down unused MultiSynths
- check the device every 2 s (`checkAlive()`) and re-initialize after a loss (`begin()`)
- set the frequency correction without re-initialization
- enable/disable outputs
- report the expected synthesized frequency per CLK (`actualHz()`)
- log SI5351 operations

---

## 27. TouchDriver Responsibilities

`TouchDriver` abstracts XPT2046 touch.

Responsibilities:

- initialize VSPI touch bus
- read XPT2046 touch events
- map raw touch coordinates
- filter jitter
- debounce touch input
- feed LVGL pointer input

Runtime calibration is disabled.

---

## 28. BacklightDriver Responsibilities

`BacklightDriver` controls the display backlight.

Responsibilities:

- initialize LEDC PWM on GPIO21
- map 0–100 % to 0–255 PWM duty (the 5 % user minimum is enforced by AppController and SettingsService)
- apply brightness changes

---

## 29. SettingsService Responsibilities

`SettingsService` handles persistent configuration.

Responsibilities:

- open Preferences namespace
- load settings
- save settings
- debounce autosave
- log save events
- report NVS unavailability and failed saves

Autosave delay:

```text
5 seconds
```

---

## 30. Storage Service

`Storage` wraps Arduino Preferences.

Purpose:

- isolate NVS API calls
- simplify future storage migration
- keep SettingsService cleaner

---

## 31. Logger

Logger output format examples:

```text
[SI5351] Init OK; all outputs off
[APP] SI5351 at 0x60, SDA=22 SCL=27
[APP] CH1 frequency: 10000000
[SI5351] Correction (0.01 ppm): 970
[SETTINGS] Saved to NVS
[ERROR][SI5351] Frequency set failed
```

`Logger::debug()` / `Logger::debugValue()` are compiled out unless `LOG_DEBUG` is 1.

---

## 32. Frequency Utilities

`FrequencyUtils` provides:

- frequency clamping
- frequency formatting (`10.700 000 MHz`)
- step formatting (`1 kHz`) and ppm formatting (`+9.70 ppm`)
- numpad frequency parsing with range check
- step preset lookup/snapping

The synthesized frequency is no longer approximated here; `SI5351Manager` computes it from the actual PLL/MultiSynth values.

Frequency range:

```text
1 MHz to 100 MHz
```

---

## 33. Build Environment

### 33.1 PlatformIO Environment

Shared ESP32 settings are in `[esp32]`; the two firmware environments extend it and only add the display controller flags (see 4.3). Two host environments run the unit tests in `test/`:

```ini
[platformio]
default_envs = esp32-2432S028R

[esp32]
platform = espressif32@6.7.0
board = esp32dev
framework = arduino
; lib_deps, shared build_flags, partitions
test_ignore = *              ; the tests run on the host only

[env:esp32-2432S028R]        ; both boards, display controller detected at boot
extends = esp32
build_flags = ${esp32.build_flags} -DILI9341_DRIVER=1

[env:native]                 ; host tests, gcc in PATH (Linux/macOS/CI)
[env:native-win]             ; host tests, PlatformIO's bundled MinGW (Windows)
```

The host environments compile only the hardware-independent sources (`AppController`, `SI5351Manager`, `I2CBus`, display detection, settings, storage, backlight, frequency utilities, logger) against the fakes in `test/fakes`; `include/constants.h`, `include/types.h` and `utils/frequency_utils.h` therefore must not include `Arduino.h`.

`default_envs` is required: without it, `pio run -t upload` would also try the host environments.

### 33.2 Required Libraries

```text
TFT_eSPI
LVGL
XPT2046_Touchscreen v1.4 from GitHub
Preferences built into Arduino-ESP32
```

The XPT2046 library is installed by GitHub URL because the PlatformIO registry version is unreliable:

```ini
https://github.com/PaulStoffregen/XPT2046_Touchscreen.git#v1.4
```

---

## 34. Safety and Constraints

Mandatory constraints:

- GPIO21 is only used for backlight PWM
- no WiFi
- no OTA
- no external buttons
- display uses HSPI
- touch uses VSPI
- SI5351 I2C pins are explicitly configured
- IQ outputs are disabled before reconfiguration; standalone outputs are retuned glitch-free
- all outputs are off after power-up
- errors do not crash the firmware

---

## 35. Known Limitations

- Touch calibration is fixed to default values.
- The displayed deviation assumes that the frequency correction is right.
- CH3 IQ accuracy is ≤ 1 Hz (standalone ≤ 0.5 Hz).
- CH3 IQ mode is not available below 4.730 MHz.

---

## 36. Future Improvement Ideas

Possible future extensions:

- preset memory
- sweep mode
- modulation modes
- external trigger on GPIO35
- calibration assistant (enter measured frequency, compute correction)
- display flush via DMA and double buffer

---

## 37. Example Use Cases

### 37.1 10 MHz Reference on CH1

```text
1. Open CH1
2. Press NUMPAD
3. Enter 10
4. Press MHz
5. Switch CH1 ON
```

Result:

```text
CLK0 = 10 MHz square wave
```

### 37.2 Two Independent Outputs

```text
CH1 = 10 MHz
CH2 = 14 MHz
```

Result:

```text
CLK0 = 10 MHz
CLK1 = 14 MHz
```

### 37.3 IQ Output

```text
CH1 = 10 MHz
CH3 master = CH1
CH3 phase = 90°
```

Result:

```text
CLK0 = 10 MHz reference
CLK2 = 10 MHz with 90° phase offset
```

---

## 38. Authoritative Current Pin Summary

| Function | GPIO |
|---|---:|
| TFT MOSI | 13 |
| TFT MISO | 12 |
| TFT SCK | 14 |
| TFT CS | 15 |
| TFT DC | 2 |
| TFT RST | -1 |
| Touch MOSI | 32 |
| Touch MISO | 39 |
| Touch SCK | 25 |
| Touch CS | 33 |
| Touch IRQ | 36 |
| SI5351 SDA | 22 |
| SI5351 SCL | 27 |
| Backlight | 21 |
| RGB LED R | 4 |
| RGB LED G | 16 |
| RGB LED B | 17 |
| Free input only | 35 |

---

## 39. Final Result

The project implements a working modular HF square-wave generator firmware with:

- color touchscreen GUI
- persistent settings
- CH1 and CH2 independent frequency outputs
- CH3 IQ follower mode
- global drive-strength setting
- safe SI5351 output switching
- serial diagnostics
- German and English user manuals
- documented hardware wiring and software architecture

---

## 40. Changes in 1.1.0 and Approved Deviations

Approved deviations from the original specification (approved by the project owner on 2026-10-26):

| Topic | Original specification | Now | Reason |
|---|---|---|---|
| Numpad (18) | No Cancel button, small 5-row grid | 4×4 grid with back button `◀` to the channel page, placeholder, input validation | Keys were ~14 px high; `MAIN` discarded the input and jumped one level too far |
| Frequency correction (17.2) | Buttons ±10 / ±1 without unit | Display in ppm, steps 0.01 / 0.1 / 1 ppm, hold-to-repeat, ±50 ppm | Unit is 0.01 ppm; typical crystal errors needed hundreds of taps |
| CH3 IQ range and phases (9.2, 10.5, 10.6) | 1–100 MHz, phase register only | 4.730–100 MHz, 180°/270° via CLK_INV | Library clamps IQ to 1.4 MHz and runs the VCO below 600 MHz under 4.725 MHz; 180°/270° were only possible from 14.3 / 21.4 MHz |
| CH3 state (10.6, 16.3) | CH3 disabled on invalid phase | CH3 request kept, state WAIT, automatic start | The user's choice was lost and CH3 never came back after a frequency change |

Further changes (within the specification):

- layout fixed: header padding, card sizes and widths, no scrollbars or clipped controls
- main page cards tappable with own switches; ON/OFF/WAIT/NO HW states
- step presets on the channel pages, hold-to-repeat for +/-, deviation display
- requested frequency stored exactly instead of the rounded SI5351 value
- backlight minimum 5 %
- message boxes close with OK, no stacking, single boot message without SI5351
- only the affected PLL is reset; correction without SI5351 re-init
- pages are built once per navigation and updated by `refresh_()`; navigation is deferred with `lv_async_call()`; enum-based action dispatch
- SI5351 library pinned to a commit; unused LVGL fonts and widgets disabled

## 41. Changes in 1.2.0

Approved by the project owner on 2026-10-27:

| Topic | Before | Now |
|---|---|---|
| Startup state | Stored ON/OFF states were restored | All outputs start off; ON/OFF is no longer stored |
| SI5351 driver | osmanovv/esp32-si5351 plus direct writes to library internals | Own register-level driver (10.1) |

Further changes:

- frequency correction applied exactly at every frequency
- standalone tuning without PLL reset and without disabling the output; ≤ 0.5 Hz error
- unused MultiSynth (CLK2 outside IQ mode) powered down
- SI5351 supervision every 2 s, automatic reconnect, `Connection lost` dialog
- backlight switched on only after the first frame
- dialogs for NVS unavailable / save failed, CONFIG indicator `Not saved` / `Save failed`; dialogs are queued instead of replaced
- channel page: tap a digit to select the step, tap the unit to open the numpad, active digit highlighted
- output error state: a setup that fails while the SI5351 answers is shown as `ERR` (MAIN card, header label, info line); the switch keeps the requested state; retried every 2 s without a further dialog. Test hook `TEST_FAULT_IQ=1` (config.h) makes the IQ setup fail at 270°
- `PIN_TFT_*` removed from `pins.h`; the display pins are defined only by the build flags
- support for the CYD2USB board (ST7789) as a second PlatformIO environment; `default_envs` selects the original CYD

## 42. Automatic SDA/SCL Detection

Requested by the project owner on 2026-10-28.

- The SI5351 cable at CN1 may be connected in either order. `I2CBus` (`hal/i2c_bus.*`) opens one of the two assignments (`I2CMap::Default`: SDA = GPIO22, `Swapped`: SDA = GPIO27); `AppController::connect_()` tries the open assignment first, then the other one.
- Detection runs at boot and in every supervision cycle while the SI5351 is not ready (hot plug, replugging during operation). While it is ready, the pins are never changed.
- Per assignment: address probe (0 ACK, 2 NACK, 5 timeout), then `SI5351Manager::begin()` with identity check. A failed init still tries the other assignment, because weak pull-ups can fake an ACK on the wrong one.
- Identity check, reads only (`SI5351Manager::identify()`): register 0 = 0xFF, register 183 = 0x00 / 0xFF or XTAL_CL = 00 reject the device (`Error::NotSi5351`). Register 183 bits 5:0 other than 010010b and a SYS_INIT that stays set only log a warning (clones). Then register 3 is written with 0xFF and read back; a mismatch rejects the device (`Error::NoReadback`). Init writes stop at the first error.
- A rejected device is marked for its assignment and not written again. After `NotSi5351` the read-only check is repeated every cycle, so an SI5351 that was still powering up is initialized once it passes. After `NoReadback` the device is left alone until it stops answering (NACK). A successful connection clears the mark.
- `LinkState`: `NotFound` (silent retry), `Blocked` (line held low), `UnknownDevice` (not programmed again until it stops answering), `Connected`. A dialog appears once per change to `Blocked` / `UnknownDevice`; at boot `main.cpp` shows the dialog for every state other than `Connected`.
- Detection clock 100 kHz, 400 kHz after a successful init. If the read-only check fails at 400 kHz (weak pull-ups), the connection stays at 100 kHz. `Wire` timeout 20 ms.
- The GUI closes connection dialogs when `AppController::connectionCount()` changes, so a reconnect between two GUI samples (300 ms) is not missed.
- At boot, a line held low (SI5351 reset during a read) is released with 9 clocks and a STOP on each pin (open drain with the internal pull-up, 10 µs half period). The debug scan stops at the first timeout.
- `I2CBus::open()` treats a failed `Wire.end()` as a closed bus: `begin()` would otherwise report success without TX buffer and the next transfer would leak the `Wire` lock.
- Arduino-ESP32 2.0.x leaks the `Wire` lock on a transfer while the bus is not started; `I2CBus::probe()` and the driver's register access check `I2CBus::isOpen()` first. The startup scan runs only with `LOG_DEBUG` and an open bus.
- The assignment is kept in RAM only, no NVS key.

## 43. Display Controller Detection

Requested by the project owner on 2026-10-28. One firmware for both boards; the `esp32-2432S028R-usbc` environment is removed.

Measured on both boards (diagnostic sketch, 2026-10-28):

| | Original CYD | CYD2USB |
|---|---|---|
| MADCTL read-back (0Bh after writing A8h / 54h) | exact, no dummy bit | exact, no dummy bit |
| RDDID (04h, one dummy bit) | `00 00 00` | `81 81 B3` (not the ST7789V datasheet value `85 85 52`) |
| RDID4 (D3h) without the 0xD9 index trick | not driven | not driven |
| MISO with internal pull-up / pull-down | data stays valid | the pull overrides the panel completely |
| Error-free read clock | up to 16 MHz | up to 8 MHz |
| Writes at 40 MHz, SPI mode 0 and 3 | error-free | error-free |

After the ILI9341 init sequence the ST7789 reports ID1 = 0x10 (C1h is IDSET there); SWRESET restores it.

Detection (`DisplayDetect::probe()`, before `tft.init()`, HSPI 1 MHz, mode 0, GPIO12 without pull):

1. SWRESET, 10 ms.
2. Write MADCTL A8h, read 0Bh; write MADCTL 54h, read 0Bh. The XOR of both reads must show FCh in bits 7:2 (shift 0 or 1). A floating or stuck MISO fails here.
3. Read RDDID twice; both reads must match. ID = 24 bits after the dummy bit.
4. ID `000000` → ILI9341; ID `FFFFFF` → unknown; any other ID → ST7789 (unknown IDs are logged as an error).
5. MADCTL back to 00h.

Without a result (step 2 or 3 failed, or ID `FFFFFF`) the probe is repeated, at most 3 attempts (about 20 ms each); the log shows the number of attempts.

Decision (`DisplayDetect::decide()`), in this order:

1. Build flag `DISPLAY_FORCE_PANEL` = 1 (ILI9341) or 2 (ST7789): used as is, nothing stored.
2. NVS key `disp_panel` (namespace `hfgen`, 0 = automatic, 1 = ILI9341, 2 = ST7789) set: stored choice.
3. Otherwise the detection; ILI9341 if nothing was detected.
4. Boot gesture (touch pressed at power-on and held for 1.5 s, ≥ 90 % of 20 ms samples): the other controller. It is stored; if it equals the automatic result, `disp_panel` is reset to 0. The backlight blinks twice (not with `DISPLAY_FORCE_PANEL`). Only presses with a plausible raw position count (`TouchDriver::pressed()`), so a missing or defective touch controller with saturated readings does not trigger the gesture on every boot.

After any touch at power-on, gesture or not, the firmware waits until the finger has been lifted for 200 ms (max. 10 s), so the touch does not reach LVGL as the first tap.

A wrong stored choice can always be undone with a second gesture, which works without a readable screen. `disp_panel` is written only after a gesture and only when it changes.

Init: TFT_eSPI sends its ILI9341 sequence on both boards. On the ST7789 `DisplayDetect::postInit()` then sends SWRESET, 150 ms, and `ST7789_INIT` (TFT_eSPI 2.5.43 `ST7789_Init.h` as built before with `TFT_RGB_ORDER=TFT_BGR`, `TFT_INVERSION_OFF`: without `fillScreen()`, INVOFF). After `setRotation()` the ST7789 gets its own MADCTL (`08 68 C8 A8` for rotation 0..3 instead of `48 28 88 E8`). CASET, RASET, RAMWR and the 16-bit color format are the same on both controllers; `setRotation()`, `readPixel()` and `readRect()` must not be called on TFT_eSPI directly.
