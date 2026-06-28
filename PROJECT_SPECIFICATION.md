# CYD HF Generator – Complete Project Specification

## 1. Project Goal

The goal of this project is to implement a maintainable, modular, touchscreen-controlled **3-channel HF square-wave generator** based on:

- **ESP32-2432S028R / Sunton CYD / Cheap Yellow Display**
- **ILI9341 320×240 TFT display**
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

If a requested frequency or phase cannot be represented safely by the SI5351 configuration, the firmware shall reject or round the value and operate safely.

---

## 2. Current Confirmed Hardware Configuration

This project uses the following confirmed wiring.

The original specification listed SI5351 I2C as `SDA=27`, `SCL=22`. During testing on the actual hardware, the working wiring was verified by I2C scan as:

```text
SDA = GPIO22
SCL = GPIO27
SI5351 address = 0x60
```

The project is therefore configured for:

```cpp
static constexpr int PIN_I2C_SDA = 22;
static constexpr int PIN_I2C_SCL = 27;
```

If a different unit is wired according to the original specification, these two values must be swapped back in:

```text
include/pins.h
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
ILI9341 TFT
Resolution: 320 × 240
Orientation: Landscape
SPI bus: HSPI
```

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

The display is configured through PlatformIO build flags, not by editing the library files.

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

The firmware always uses fixed default calibration values:

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
| SDA | GPIO22 |
| SCL | GPIO27 |
| VCC | 3.3 V |
| GND | GND |

I2C clock:

```text
400 kHz
```

The bus is scanned during startup. Expected serial output:

```text
[I2C] Found device at 0x60
```

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
0 % to 100 %
```

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
- CH3 output is active only if:
  - CH3 is ON
  - selected master channel is ON
  - selected phase/frequency combination is valid

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

### 10.1 Library

The project uses:

```text
https://github.com/osmanovv/esp32-si5351.git
```

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

When changing frequency, the firmware shall switch safely:

```text
1. Disable output
2. Reconfigure SI5351 / PLL / MultiSynth
3. Apply frequency
4. Re-enable output if channel is ON
```

Switching artifacts are allowed.

### 10.5 CH3 IQ Mode

CH3 must share the same PLL as its selected master.

When CH3 is enabled:

- master and CH3 are configured as an IQ pair
- same frequency
- phase offset applied to CH3

When CH3 is disabled:

- CLK2 is disabled
- CH1 and CH2 are configured as standalone outputs

### 10.6 Phase Validation

The SI5351 phase offset register is 7-bit.

Therefore not every requested phase is valid at every frequency.

The firmware validates phase feasibility before applying CH3 IQ mode.

If phase is invalid:

- CH3 is disabled safely
- error is logged
- LVGL message box is shown

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
- ON/OFF switch in the title bar on channel pages

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

The main page displays:

- CH1 frequency and ON/OFF state
- CH2 frequency and ON/OFF state
- CH3 frequency, master, phase and ON/OFF state
- colored navigation buttons:
  - CH1 red
  - CH2 green
  - CH3 blue
  - CONFIG yellow

The project title header is intentionally removed to maximize available status space.

---

## 14. CH1 Page

### 14.1 Purpose

Control CH1 frequency and output state.

### 14.2 Elements

- Header with CH1 title
- ON/OFF switch in title row
- Frequency display
- Delta-step display
- `+` button
- `-` button
- `NUMPAD` button
- `MAIN` button

### 14.3 Behavior

- `+` increases CH1 frequency by delta step
- `-` decreases CH1 frequency by delta step
- `NUMPAD` opens numeric frequency entry
- ON/OFF switch enables/disables CLK0

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
- master selection:
  - CH1
  - CH2
- phase selection:
  - 0
  - 90
  - 180
  - 270


### 16.3 Behavior

CH3 follows its master frequency.

If CH3 is ON but its master is OFF, CH3 output remains OFF. Displayed as OFF.

---

## 17. CONFIG Page

### 17.1 Purpose

The CONFIG page contains global device settings.

### 17.2 Elements

- Global drive-strength dropdown:
  - 2 mA
  - 4 mA
  - 6 mA
  - 8 mA
- Backlight slider
- Delta step controls:
  - x10
  - /10
- Frequency correction controls:
  - +10
  - +1
  - -10
  - -1

### 17.3 Touch Calibration

Touch calibration UI is disabled.

There is no touch-calibration button.

---

## 18. NUMPAD Page

### 18.1 Purpose

Numeric frequency input for CH1 and CH2.

### 18.2 Elements

- numeric buttons `0` to `9`
- decimal point `.`
- backspace `<--`
- `Clear`
- unit buttons:
  - `Hz`
  - `kHz`
  - `MHz`

There is no separate OK button and no Cancel button. 

### 18.3 Behavior

Pressing a unit button confirms the input immediately.

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
- CH1 ON/OFF
- CH2 ON/OFF
- CH3 ON/OFF
- CH3 master selection
- CH3 phase
- backlight percentage
- delta step
- frequency correction
- global drive strength
- touch calibration


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
- I2C scan results
- SI5351 probe result
- SI5351 init
- frequency changes
- output mask changes
- settings save events
- touch initialization
- SI5351 phase errors

### 20.2 GUI Error Reporting

On important errors, an LVGL message box is shown.

Examples:

- SI5351 not detected
- invalid CH3 phase/frequency combination
- SI5351 frequency setup error

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
|   |   ├── touch.cpp
|   |   └── touch.h
|   |
|   ├── gui/
|   |   ├── gui.cpp
|   |   ├── gui.h
|   |   ├── pages/
|   |   |   ├── page_main.cpp
|   |   |   ├── page_ch1.cpp
|   |   |   ├── page_ch2.cpp
|   |   |   ├── page_ch3.cpp
|   |   |   ├── page_config.cpp
|   |   |   └── page_numpad.cpp
|   |   └── widgets/
|   |       ├── freq_display.cpp
|   |       └── freq_display.h
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
2. I2C bus initialization and scan
3. Backlight PWM
4. Settings load from NVS
5. SI5351 initialization and probe
6. Touch initialization
7. LVGL initialization
8. GUI initialization
9. Apply stored channel settings
```

The main loop:

```cpp
void loop() {
    lv_timer_handler();
    touch.process();
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
    uint64_t frequency;
    bool enabled;
    uint8_t drive_strength;
};
```

### 24.2 Channel3

```cpp
struct Channel3 {
    bool enabled;
    uint8_t master;     // 1 or 2
    uint16_t phase;     // 0/90/180/270
    uint8_t drive_strength;
};
```

Note: CH3 contains `drive_strength` for internal state compatibility, but the user-facing drive-strength setting is global.

### 24.3 SystemSettings

```cpp
struct SystemSettings {
    uint32_t delta_step;
    int32_t freq_correction;
    uint8_t backlight;
    uint8_t default_drive_strength;
};
```

---

## 25. AppController Responsibilities

`AppController` is the central state owner.

Responsibilities:

- hold current channel state
- validate channel numbers and settings
- clamp/round frequency requests
- coordinate SI5351 reconfiguration
- coordinate CH3 follow logic
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
- probe I2C address 0x60
- configure standalone frequency outputs
- configure IQ pair for CH3
- enable/disable outputs
- cache and apply drive strength
- validate phase offset
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
- map 0–100 % to 0–255 PWM duty
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
[I2C] Found device at 0x60
[SI5351] Init OK
[SI5351] Set Freq CLK0: 10000000
[SETTINGS] Saved to NVS
[ERROR][SI5351] Selected CH3 phase is not valid
```

---

## 32. Frequency Utilities

`FrequencyUtils` provides:

- frequency clamping
- SI5351-near actual frequency approximation
- frequency formatting
- numpad frequency parsing

Frequency range:

```text
1 MHz to 100 MHz
```

---

## 33. Build Environment

### 33.1 PlatformIO Environment

```ini
[env:esp32-2432S028R]
platform = espressif32@6.7.0
board = esp32dev
framework = arduino
```

### 33.2 Required Libraries

```text
TFT_eSPI
LVGL
XPT2046_Touchscreen v1.4 from GitHub
osmanovv/esp32-si5351 from GitHub
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
- outputs are disabled before SI5351 reconfiguration
- errors do not crash the firmware

---

## 35. Known Limitations

- Touch calibration is fixed to default values.
- Exact SI5351 output frequency is approximated based on divider calculations.
- The external SI5351 library does not expose full I2C write-status details for every register write.
- Some CH3 phase/frequency combinations are impossible due to the SI5351 7-bit phase register.
- GUI pages are modularized, but callbacks still use string action identifiers.

---

## 36. Future Improvement Ideas

Possible future extensions:

- preset memory
- sweep mode
- modulation modes
- external trigger on GPIO35
- improved exact SI5351 frequency calculation
- enum-based GUI action dispatch instead of string callbacks
- optional compile-time log levels
- optional SI5351 library fork with complete I2C error reporting

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

