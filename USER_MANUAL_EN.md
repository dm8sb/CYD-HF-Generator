# User Manual – CYD HF Generator

Firmware version: 1.2.0

## 1. Overview

The **CYD HF Generator** is a 3-channel HF square-wave generator based on an ESP32-2432S028R display board and an SI5351 clock generator.

It generates 50 % duty-cycle square waves in the range:

```text
1 MHz to 100 MHz
```

The three outputs are:

| Output | Function |
|---|---|
| CH1 / CLK0 | Freely adjustable frequency |
| CH2 / CLK1 | Freely adjustable frequency |
| CH3 / CLK2 | IQ follower of CH1 or CH2 |

CH3 always follows the frequency of its selected master and can use a phase offset of 0°, 90°, 180° or 270°. IQ operation is available from **4.730 MHz to 100 MHz**; all four phases are available in that range.

---

## 2. Basic Operation

After power-up, the main page is displayed. **All outputs are off after power-up**, even if they were on when the device was switched off. Frequencies, CH3 master and phase are kept.

The main page shows one card per channel with:

- channel name and status
- frequency
- its own ON/OFF switch
- extra info (CH3: master, phase and, if applicable, the reason for `WAIT`)

Operation on the main page:

- **Tap a card** to open that channel's page.
- **The switch on a card** turns the output on or off directly, without leaving the page.
- The buttons at the bottom open CH1, CH2, CH3 and CONFIG.

Status below the channel name:

| Status | Meaning |
|---|---|
| `ON` (green) | Output active |
| `OFF` (gray) | Output off |
| `ERR` (red) | Setting up the output failed although the SI5351 answers; the firmware retries every 2 s. The switch keeps showing the requested state |
| `WAIT` (orange) | CH3 is switched on but cannot run right now (master off or master frequency below 4.730 MHz) |
| `NO HW` (red) | SI5351 not found or connection lost; the firmware reconnects automatically |

The navigation buttons are color-coded:

| Button | Color |
|---|---|
| CH1 | Red |
| CH2 | Green |
| CH3 | Blue |
| CONFIG | Yellow |

On all subpages the title bar contains the ON/OFF switch (channel pages) with an `ON`/`OFF` state label, and on the right the `MAIN` button to return to the main page.

The RGB LED also shows the channel color on channel pages:

| Page | RGB LED |
|---|---|
| CH1 | Red |
| CH2 | Green |
| CH3 | Blue |

---

## 3. Frequency Display

Frequencies are shown in MHz with grouped decimal digits.

Examples:

```text
1.000 000 MHz
5.000 000 MHz
10.700 000 MHz
100.000 000 MHz
```

Meaning:

| Display | Frequency |
|---|---:|
| 1.000 000 MHz | 1 MHz |
| 5.000 000 MHz | 5 MHz |
| 10.700 000 MHz | 10.7 MHz |
| 100.000 000 MHz | 100 MHz |

The display and settings hold the **requested frequency** with 1 Hz resolution. The SI5351 cannot synthesize every frequency exactly; the calculated deviation is shown in the info line of the channel page (see section 5).

---

## 4. Using CH1 and CH2

CH1 and CH2 are freely adjustable frequency outputs.

### Turning CH1 or CH2 ON

1. On the main page, tap the card or the `CH1` / `CH2` button.
2. Turn on the ON/OFF switch in the title bar.
3. The display changes to `ON`.
4. The corresponding SI5351 output is active:

| Channel | SI5351 Output |
|---|---|
| CH1 | CLK0 |
| CH2 | CLK1 |

Alternatively, use the switch on the channel's card on the main page.

### Turning CH1 or CH2 OFF

1. Open the channel page (or use the switch on the main page).
2. Turn off the ON/OFF switch.
3. The display changes to `OFF`.
4. The output is disabled.

---

## 5. Changing Frequency with + and -

The CH1 and CH2 pages contain:

- the frequency card with an info line
- a row of step buttons: `1`, `10`, `100`, `1k`, `10k`, `100k`, `1M` (in Hz)
- large `-` (left) and `+` (right) buttons
- the `NUMPAD` button

The `-` and `+` buttons change the frequency by the selected step. The selected step is highlighted and can be chosen directly on the channel page (or in CONFIG).

**Tap a digit:** Tapping a digit of the frequency display selects that digit as the step (e.g. the third decimal = 1 kHz). The digit changed by `-`/`+` is highlighted in the channel color. Digits above 1 MHz (10 MHz, 100 MHz) select the largest step, 1 MHz. Tapping the unit `MHz` opens the NUMPAD.

Below about 110 MHz, frequency changes do not interrupt the output. Only in rare cases the PLL has to be switched, which causes a short dropout.

**Press and hold:** after about 0.4 s the step repeats automatically about every 0.1 s until the button is released.

The info line shows:

```text
Step 1 kHz  |  Deviation +0.3 Hz
```

- `Step` = current step
- `Deviation` = calculated deviation of the frequency synthesized by the SI5351 from the requested frequency (rounding of PLL/divider values, assuming the frequency correction is right)

Example:

```text
Current frequency: 10.000 000 MHz
Step: 1k
Press +
New frequency: 10.001 000 MHz
```

Another example:

```text
Current frequency: 7.100 000 MHz
Step: 10k
Press -
New frequency: 7.090 000 MHz
```

The frequency always stays within 1 MHz to 100 MHz.

---

## 6. Entering Frequency with the NUMPAD

On CH1 or CH2:

1. Press `NUMPAD`.
2. Enter digits. The current frequency is shown in gray as a placeholder in the input field.
3. Optionally use a decimal point (only one is possible).
4. Press a unit button:
   - `Hz`
   - `kHz`
   - `MHz`

The unit button confirms the entry immediately and returns to the channel page.

The `◀` button at the top left returns to the channel page **without changes**.

### Keypad

```text
 7   8   9   MHz
 4   5   6   kHz
 1   2   3   Hz
 .   0   ⌫   C
```

| Button | Function |
|---|---|
| 0–9 | Digit entry |
| . | Decimal point |
| `⌫` | Delete last character |
| C | Clear input |
| Hz | Apply value as Hz |
| kHz | Apply value as kHz |
| MHz | Apply value as MHz |
| `◀` | Back to the channel page without changes |

Maximum input length is 9 characters.

### Invalid Input

If the value is outside 1–100 MHz, or the input is empty or invalid, it is **not applied**. The input field is cleared, its border turns red and the hint `1 - 100 MHz` appears. The next key starts a new entry.

### Example 1: Enter 10 MHz

1. Open CH1.
2. Press `NUMPAD`.
3. Enter:

```text
10
```

4. Press `MHz`.
5. CH1 is set to 10 MHz.

Display:

```text
10.000 000 MHz
```

### Example 2: Enter 10.7 MHz

1. Open CH2.
2. Press `NUMPAD`.
3. Enter:

```text
10.7
```

4. Press `MHz`.

Display:

```text
10.700 000 MHz
```

### Example 3: Enter 1000 kHz

1. Open CH1.
2. Press `NUMPAD`.
3. Enter:

```text
1000
```

4. Press `kHz`.

Display:

```text
1.000 000 MHz
```

### Example 4: Enter 5,000,000 Hz

1. Open CH1.
2. Press `NUMPAD`.
3. Enter:

```text
5000000
```

4. Press `Hz`.

Display:

```text
5.000 000 MHz
```

---

## 7. Using CH3 as IQ Follower

CH3 is not a freely adjustable channel. CH3 follows either CH1 or CH2.

On the CH3 page you can set:

- Master: buttons `CH1` or `CH2`
- Phase: buttons `0`, `90`, `180` or `270` (degrees)
- ON/OFF via the switch in the title bar

The selected button is highlighted. The CH3 frequency is always identical to the master frequency.

### Status Line

Below the frequency, the CH3 page shows the current state:

| Display | Meaning |
|---|---|
| `Off  \|  follows CHx` | CH3 switched off |
| `IQ active, follows CHx` | IQ output running |
| `Waiting: master CHx is off` | CH3 on, master off |
| `Waiting: CHx below 4.730 MHz` | CH3 on, master frequency too low |
| `SI5351 not found` | SI5351 missing |

**Important:** If CH3 is switched on but cannot run right now (master off or below 4.730 MHz), the switch stays ON. Output CLK2 stays off until the conditions are met — then CH3 starts **automatically**. No error message is shown.

### Frequency Range and Phases

| Phase | Available |
|---|---|
| 0° | 4.730 – 100 MHz |
| 90° | 4.730 – 100 MHz |
| 180° | 4.730 – 100 MHz |
| 270° | 4.730 – 100 MHz |

180° and 270° are produced by inverting the CH3 output (0° or 90° plus inversion). Below 4.730 MHz the SI5351 oscillator (VCO) would run below 600 MHz, outside its specification.

### Example: CH3 follows CH1 with 90° phase

1. Set CH1 to 10 MHz.
2. Turn CH1 ON.
3. Open the CH3 page.
4. Tap master `CH1`.
5. Tap phase `90`.
6. Turn CH3 ON.

Result:

```text
CH1 / CLK0 = 10 MHz, 0° reference
CH3 / CLK2 = 10 MHz, shifted by 90°
```

### Example: CH3 follows CH2 with 180° phase

1. Set CH2 to 5 MHz.
2. Turn CH2 ON.
3. Open CH3.
4. Tap master `CH2`.
5. Tap phase `180`.
6. Turn CH3 ON.

Result:

```text
CH2 / CLK1 = 5 MHz
CH3 / CLK2 = 5 MHz, shifted by 180°
```

---

## 8. CONFIG Page

The CONFIG page contains global settings.

### Backlight

The slider sets the display brightness:

```text
5 % to 100 %
```

The current value is shown to the right of the slider. The 5 % minimum prevents the display from accidentally going completely dark and becoming unusable. A stored value below 5 % (e.g. from firmware 1.0.0) is reset to 80 % at startup.

### Step (Frequency Step)

The step defines how much `+` and `-` change the frequency. It uses the same presets as the channel pages:

```text
1 Hz, 10 Hz, 100 Hz, 1 kHz, 10 kHz, 100 kHz, 1 MHz
```

| Button | Function |
|---|---|
| x10 | Next larger preset |
| /10 | Next smaller preset |

Example:

```text
Step: 1 kHz
Press x10
Step: 10 kHz
```

### Frequency Correction (Corr.)

The correction compensates the SI5351 crystal tolerance. It is shown in **ppm**, e.g. `+9.70 ppm`.

| Element | Function |
|---|---|
| Step button (`0.01` / `0.1` / `1`) | Step size in ppm; each tap selects the next one |
| `-` / `+` | Change the correction by the step size; hold to repeat |

Range: ±50 ppm. A positive value means the output was too high without correction.

The outputs are reconfigured after each change; the SI5351 is not re-initialized.

**Calibration:**

1. Set CH1 to 10 MHz and turn it on.
2. Measure with a frequency counter.
3. Deviation in Hz divided by 10 = correction in ppm.
4. Add this value to the current correction.

Example:

```text
Measured: 10,000,097 Hz  ->  +97 Hz at 10 MHz  ->  set +9.70 ppm
```

### Drive all

Drive strength is global for all channels.

Options:

```text
2 mA
4 mA
6 mA
8 mA
```

This setting applies to:

```text
CH1
CH2
CH3
```

CH3 has no independent drive strength setting.

### Save Indicator

The right side of the `Drive all` row shows the save state:

| Display | Meaning |
|---|---|
| `Saving...` | Change not yet saved (autosave after 5 s) |
| `Saved` | Everything saved |
| `Save failed` (red) | Writing to flash failed; a dialog is shown as well |
| `Not saved` (red) | Flash storage (NVS) unavailable at startup; changes are lost at power-off |

---

## 9. Saving Settings

Changes are saved automatically.

Sequence:

1. Change a setting.
2. Wait 5 seconds (on CONFIG the indicator changes from `Saving...` to `Saved`).
3. The firmware saves to NVS.
4. The serial monitor shows:

```text
[SETTINGS] Saved to NVS
```

Saved values include:

- CH1 frequency
- CH2 frequency
- CH3 master
- CH3 phase
- Backlight
- Frequency step
- Frequency correction
- Global drive strength

Touch calibration is not saved. Default values are always used.

The ON/OFF state of the outputs is not saved; all outputs are off after power-up.

---

## 10. Usage Examples

### Example A: Simple 10 MHz Output on CH1

1. Open the main page.
2. Press `CH1`.
3. Press `NUMPAD`.
4. Enter `10`.
5. Press `MHz`.
6. Set CH1 to `ON`.

Output:

```text
CLK0 = 10 MHz square wave
```

---

### Example B: Two Independent Frequencies

Goal:

```text
CH1 = 10 MHz
CH2 = 14 MHz
```

Procedure:

1. Open CH1.
2. Enter 10 MHz.
3. Turn CH1 ON.
4. Return to the main page.
5. Open CH2.
6. Enter 14 MHz.
7. Turn CH2 ON.

Outputs:

```text
CLK0 = 10 MHz
CLK1 = 14 MHz
```

---

### Example C: 10 MHz IQ Output with 90° Phase

Goal:

```text
CH1 = 10 MHz
CH3 = 10 MHz with 90° phase shift
```

Procedure:

1. Set CH1 to 10 MHz.
2. Turn CH1 ON.
3. Open CH3.
4. Tap master `CH1`.
5. Tap phase `90`.
6. Turn CH3 ON.

Outputs:

```text
CLK0 = 10 MHz reference
CLK2 = 10 MHz, shifted by 90°
```

---

### Example D: Change Output Drive Strength

Goal:

```text
All channels with 8 mA drive strength
```

Procedure:

1. Open CONFIG.
2. Select `8 mA` under `Drive all`.
3. Wait until `Saved` is shown on the right.

The setting now applies to all channels.

---

## 11. Troubleshooting

### Error Messages

Errors are shown as a dialog. `OK` closes it. At most one dialog is open at a time; further messages follow one after another, identical messages appear only once.

### SI5351 Not Found

If the SI5351 is not found at startup, **one** message is shown. Afterwards the main page shows `NO HW` for the channels and the channel pages show `SI5351 not found`. Settings can still be changed and are saved.

The firmware checks every 2 seconds whether the SI5351 answers. When it is connected later or a loose contact is fixed, the firmware reconnects automatically, restores the set frequencies and ON/OFF states and closes the connection message. If the connection is lost during operation (or the SI5351 briefly loses power), the message `Connection lost` is shown and the channels show `NO HW` until the connection is back.

SDA and SCL may be connected to CN1 in either order (GPIO22/GPIO27). The firmware tries both assignments, also after replugging during operation. The serial monitor shows the assignment found, with `(swapped)` for reversed wires.

Further messages when connecting:

| Message | Meaning |
|---|---|
| `SI5351 not found ...` | No device at address 0x60: check the cable, 3.3 V and GND. The search continues. |
| `I2C line stays low ...` | An I2C line is held at GND: short circuit, missing 3.3 V at the module or defective module. The search continues. |
| `The device at I2C address 0x60 is not an SI5351 ...` | Another device answers at 0x60. It is not programmed (at most one test byte in register 3) until it is unplugged. An SI5351 that briefly returns wrong values while being plugged in is checked again automatically. |

Note: if an SI5351 module stays powered while its cable is unplugged, its outputs keep running. On reconnect the firmware switches them off for a few milliseconds and then restores them.

### Settings Are Not Saved

If the flash storage (NVS) is unavailable at startup, a dialog is shown and CONFIG shows `Not saved`. If saving fails, a dialog is shown and CONFIG shows `Save failed`.

### No Output Signal

Check:

1. Is the channel set to `ON`?
2. Are you measuring the correct output?

```text
CH1 = CLK0
CH2 = CLK1
CH3 = CLK2
```

3. Is the SI5351 detected on I2C?

The serial monitor should show:

```text
[APP] SI5351 at 0x60, SDA=22 SCL=27
```

4. Is there a common ground?
5. Is the SI5351 powered with 3.3 V?
6. If the SI5351 module has an OE pin, is it at the active level?

Many SI5351 modules use active-low OE:

```text
OE = GND  -> outputs enabled
OE = HIGH -> outputs disabled
```

### CH3 Shows WAIT / "Waiting"

Possible causes:

```text
The master is switched off.
The master frequency is below 4.730 MHz.
```

Solution:

- turn the master on
- set the master frequency to at least 4.730 MHz

CH3 then starts automatically; the switch does not need to be operated again.

---

## 12. Serial Diagnostics

Serial monitor:

```bash
pio device monitor -b 115200
```

Important messages:

```text
[SI5351] Init OK; all outputs off
[APP] SI5351 at 0x60, SDA=22 SCL=27
[APP] CH1 frequency: ...
[SI5351] Correction (0.01 ppm): ...
[SETTINGS] Saved to NVS
```

Detailed SI5351 messages (output mask, phase register, etc.) only appear if the firmware is built with `LOG_DEBUG 1` (in `include/config.h`). They are off in normal use because serial output slows down the user interface.

---

## 13. Current Hardware Pinout

The SI5351 is connected to CN1. SDA and SCL may be swapped; the firmware detects the assignment:

```text
SDA / SCL = GPIO22 / GPIO27 (either order; tried first: SDA = GPIO22)
SI5351 address = 0x60
```

Display and touch use separate SPI buses.

### Board Variant and Matching Firmware

Two boards are sold as ESP32-2432S028R. They differ only in the display controller, and each needs its own firmware:

| Board | How to recognize it | Display controller | Firmware (PlatformIO environment) |
|---|---|---|---|
| Original CYD | micro-USB port only | ILI9341 | `esp32-2432S028R` |
| CYD2USB | additional USB-C port | ST7789 | `esp32-2432S028R-usbc` |

If the display shows only noise after flashing, the firmware of the other variant is installed. Flash the matching firmware (see README, section "Build and Flash"). Pinout, touch and operation are the same on both variants.

---

## 14. Safety Note

The SI5351 outputs are digital square-wave signals. Before connecting them to sensitive HF circuits, check:

- signal level
- load impedance
- possible attenuation
- galvanic connection
- ground routing
