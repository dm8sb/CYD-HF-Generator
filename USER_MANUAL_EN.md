# User Manual – CYD HF Generator

Firmware version: 1.3.0

This manual is a translation of [USER_MANUAL_DE.md](USER_MANUAL_DE.md) (firmware 1.3.0); the German manual is the reference for the firmware's behavior.

## 1. Overview

The **CYD HF Generator** is a 3-channel HF square-wave generator based on an ESP32-2432S028R display and an SI5351 clock generator.

The device generates square waves with a 50 % duty cycle in the range:

```text
1 MHz to 100 MHz
```

The three outputs are:

| Output | Function |
|---|---|
| CH1 / CLK0 | Freely adjustable frequency |
| CH2 / CLK1 | Freely adjustable frequency |
| CH3 / CLK2 | IQ follower of CH1 or CH2 |

CH3 always follows the frequency of its master and can be operated with a phase of 0°, 90°, 180° or 270°. IQ operation is possible from **4.730 MHz to 100 MHz**; all four phases are available in this range.

The CONFIG page also lets you select the SI5351 divider scheme (`Tuning`), spread spectrum for CH1 and the state of switched-off outputs (section 8).

---

## 2. Basic Operation

After power-up, the display stays dark until the first screen has been drawn; then the main page appears. **All outputs are off after power-up**, even if they were on when the device was switched off. Frequencies, CH3 master and phase, and all CONFIG settings are retained.

The main page shows one card per channel with:

- channel name and status
- the frequency you set (requested frequency)
- its own ON/OFF switch
- an extra line: on CH1/CH2 `IQ master of CH3` when CH3 is switched on and follows this channel; on CH3 the master and phase, e.g. `CH1 / 90 deg`, and with `WAIT` and `ERR` also the reason (e.g. `CH1 / 90 deg / master off`)

In mode `Tuning` = `Integer`, the frequency is preceded by a `~` when the actual frequency deviates from the requested frequency by 1 Hz or more (section 8).

Operation on the main page:

- **Tapping a card** opens the channel's page.
- **The switch on a card** turns the output on or off directly, without leaving the page.
- The buttons at the bottom lead to CH1, CH2, CH3 and CONFIG.

Status display below the channel name:

| Status | Meaning |
|---|---|
| `ON` (green) | Output active |
| `OFF` (gray) | Output off |
| `ERR` (red) | Setting up the output failed although the SI5351 responds; the firmware retries every 2 s. The switch keeps showing the desired setting |
| `WAIT` (orange) | CH3 is switched on but cannot run at the moment (master off or master frequency below 4.730 MHz) |
| `NO HW` (red) | SI5351 not found or connection lost; the firmware reconnects automatically |

The navigation buttons are color-coded:

| Button | Color |
|---|---|
| CH1 | Red |
| CH2 | Green |
| CH3 | Blue |
| CONFIG | Yellow |

The channel pages and CONFIG have a title bar with the `MAIN` button on the right to return to the main page. On the channel pages, it also contains the ON/OFF switch with the state label `ON`, `OFF` or `ERR`. The NUMPAD has the `◀` button instead (section 6).

On the channel pages, the RGB LED also shows the channel color:

| Page | RGB LED |
|---|---|
| CH1 | Red |
| CH2 | Green |
| CH3 | Blue |
| MAIN, CONFIG, NUMPAD | off |

---

## 3. Frequency Display

The frequency is shown in MHz with grouped decimal places.

Examples:

```text
1.000 000 MHz
5.000 000 MHz
10.700 000 MHz
100.000 000 MHz
```

This corresponds to:

| Display | Frequency |
|---|---:|
| 1.000 000 MHz | 1 MHz |
| 5.000 000 MHz | 5 MHz |
| 10.700 000 MHz | 10.7 MHz |
| 100.000 000 MHz | 100 MHz |

What is displayed and saved is the **requested frequency you set**, with 1 Hz resolution. The SI5351 cannot generate every frequency exactly; the actual frequency is shown in the `Out` line of the channel page (section 5).

---

## 4. Using CH1 and CH2

CH1 and CH2 are freely adjustable frequency outputs.

### Turning CH1 or CH2 ON

1. On the main page, tap the card or the `CH1` or `CH2` button.
2. Turn on the ON/OFF switch in the title bar.
3. The display changes to `ON`.
4. A signal is present at the corresponding SI5351 output:

| Channel | SI5351 output |
|---|---|
| CH1 | CLK0 |
| CH2 | CLK1 |

Alternatively, the channel can be switched on directly with the switch on its card on the main page.

### Turning CH1 or CH2 OFF

1. Open the channel page (or use the switch on the main page).
2. Turn off the ON/OFF switch.
3. The display changes to `OFF`.
4. The output is switched off and takes the state selected under `Off state` (default `Low`, section 8).

---

## 5. Changing the Frequency with + and −

The CH1 and CH2 channel pages contain:

- the frequency card with the `Out` line
- a row of step buttons: `1`, `10`, `100`, `1k`, `10k`, `100k`, `1M` (in Hz)
- large `−` (left) and `+` (right) buttons
- the `NUMPAD` button

The `−` and `+` buttons change the frequency by the currently selected step. The selected step is highlighted in color and is chosen directly on the channel page (or in CONFIG).

**Tapping a digit directly:** Tapping a digit of the frequency display selects that digit position as the step (e.g. the third decimal place = 1 kHz). The digit changed by `−`/`+` is highlighted in the channel color. Digits above 1 MHz (10 MHz, 100 MHz) select the largest step, 1 MHz. Tapping the unit `MHz` opens the NUMPAD.

**Press and hold:** After about 0.4 s, the step repeats automatically about every 0.1 s until the button is released.

Whether a frequency change briefly interrupts the output depends on `Tuning` (section 8):

- `Fractional` (default): no interruption. Only in rare cases does the PLL have to be switched; this causes a short dropout.
- `Low noise`: short dropout when the divider changes.
- `Integer`: short dropout when the PLL changes.
- In IQ operation, the master and CH3 drop out briefly on every frequency change (section 7).

### Actual Frequency (`Out`)

Below the frequency, the `Out` line shows the frequency that the SI5351 actually generates with the programmed dividers, and in parentheses the deviation from the requested frequency:

```text
Out 10.000 000 MHz (+0.3 Hz)
```

- The values assume that the frequency correction (section 8) is right.
- From a deviation of 1 Hz upward, the line is shown in orange.
- Deviation per mode: `Fractional` ≤ 0.5 Hz, `Low noise` up to ≈ 1.4 Hz. In `Integer` mode, the frequency lies on a grid and can deviate by a few kHz, e.g. at 7.1 MHz:

```text
Out 7.102 273 MHz (+2272.7 Hz)
```

- With spread spectrum on CH1, the line shows the mean frequency and the setting:

```text
Out 9.975 000 MHz mean, SSC -0.5 %
```

In case of errors, the following is shown in red instead of the frequency:

| Display | Meaning |
|---|---|
| `SI5351 not found` | SI5351 missing or connection lost (status `NO HW`) |
| `Output error, retrying` | Setting failed; the firmware retries every 2 s (status `ERR`) |

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
Press −
New frequency: 7.090 000 MHz
```

The frequency always stays within the range 1 MHz to 100 MHz.

---

## 6. Entering the Frequency with the NUMPAD

On CH1 or CH2:

1. Press `NUMPAD`.
2. Enter digits. The current frequency is shown in gray as a placeholder in the input field.
3. Optionally use a decimal point (only one is allowed).
4. Press a unit:
   - `Hz`
   - `kHz`
   - `MHz`

The unit confirms the entry immediately and returns to the channel page.

The `◀` button at the top left returns to the channel page **without any change**.

### Keypad

```text
 7   8   9   MHz
 4   5   6   kHz
 1   2   3   Hz
 .   0   ⌫   C
```

| Key | Function |
|---|---|
| 0–9 | Digit entry |
| . | Decimal point |
| `⌫` | Delete last character |
| C | Clear input |
| Hz | Apply value as Hz |
| kHz | Apply value as kHz |
| MHz | Apply value as MHz |
| `◀` | Back to the channel page without any change |

A maximum of 9 characters is allowed.

### Invalid Input

If the value is outside 1–100 MHz, or the input is empty or invalid, it is **not applied**. The input field is cleared, its border turns red and the hint `1 - 100 MHz` appears. With the next key press you can start a new entry.

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

### Example 3: Enter 1 000 kHz

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

### Example 4: Enter 5 000 000 Hz

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

CH3 is not a freely adjustable channel. CH3 follows CH1 or CH2.

On the CH3 page you can set:

- Master: buttons `CH1` or `CH2`
- Phase: buttons `0`, `90`, `180` or `270` (degrees)
- ON/OFF via the switch in the title bar

The selected button is highlighted in color. The CH3 frequency is always identical to the master frequency; the drive strength is set in CONFIG for all channels.

### Status Line

Below the frequency, the CH3 page shows the current state:

| Display | Meaning |
|---|---|
| `Off  \|  follows CHx` | CH3 switched off |
| `Out 10.000 000 MHz (+0.3 Hz)` | IQ output running; actual frequency of CLK2 (section 5) |
| `Waiting: master CHx is off` | CH3 switched on, master off |
| `Waiting: CHx below 4.730 MHz` | CH3 switched on, master frequency too low (higher with spread spectrum on CH1, e.g. `4.749`) |
| `Waiting: CHx above 99.502 MHz` | CH3 switched on, master frequency too high; only with spread `+/-0.5 %` on master CH1 |
| `IQ setup failed, retrying` | Setting up the IQ pair failed (status `ERR`); CLK2 is off, the master keeps running, the firmware retries every 2 s |
| `SI5351 not found` | SI5351 missing |

**Important:** If CH3 is switched on but cannot run at the moment (master off or below 4.730 MHz), the switch stays ON. Output CLK2 stays off until the conditions are met – then CH3 starts **automatically**. No error message appears.

### Frequency Range and Phases

| Phase | Available |
|---|---|
| 0° | 4.730 – 100 MHz |
| 90° | 4.730 – 100 MHz |
| 180° | 4.730 – 100 MHz |
| 270° | 4.730 – 100 MHz |

180° and 270° are produced by inverting the CH3 output (0° or 90° plus inversion). Below 4.730 MHz, the SI5351 oscillator (VCO) would run below 600 MHz and thus outside its specification.

Accuracy: In `Fractional` and `Low noise` modes, the IQ frequency deviates from the requested frequency by at most ≈ 1.4 Hz. In `Integer` mode, IQ operation also uses an integer PLL; the frequency then lies on the grid (section 8), and the phases work unchanged.

### Tuning in IQ Operation

The master and CH3 share one PLL. Every change of the master frequency or of the frequency correction, and every phase change from 0°/180° to 90°/270° (or vice versa), briefly switches both outputs off, reconfigures them and realigns the phase by resetting the PLL. Switching between 0° and 180° or between 90° and 270°, and changing the drive strength, work without a dropout.

Spread spectrum (`Spread CH1`) also affects CH3 when CH1 is its master; with master CH2, CH3 is not affected.

### Example: CH3 Follows CH1 with 90° Phase

1. Set CH1 to 10 MHz.
2. Switch CH1 on.
3. Go to the CH3 page.
4. Tap master `CH1`.
5. Tap phase `90`.
6. Switch CH3 on.

Result:

```text
CH1 / CLK0 = 10 MHz, 0° reference
CH3 / CLK2 = 10 MHz, shifted by 90°
```

### Example: CH3 Follows CH2 with 180° Phase

1. Set CH2 to 5 MHz.
2. Switch CH2 on.
3. Open CH3.
4. Tap master `CH2`.
5. Tap phase `180`.
6. Switch CH3 on.

Result:

```text
CH2 / CLK1 = 5 MHz
CH3 / CLK2 = 5 MHz, shifted by 180°
```

---

## 8. CONFIG Page

The CONFIG page is where global settings are changed. Every change takes effect immediately and is saved automatically (section 9).

### Layout and Scrolling

The title bar with the save indicator and `MAIN` stays fixed. The area below it is taller than the screen and can be moved vertically with a finger. CONFIG is the only page that scrolls.

| Row | Controls |
|---|---|
| `Backlight` | `−`, value, `+` |
| `Step` | value, `/10`, `x10` |
| `Corr.` | value in ppm, step button, `−`, `+` |
| `Drive all` | drop-down list `2 mA` … `8 mA` |
| `Tuning` | drop-down list `Fractional`, `Low noise`, `Integer` |
| `Spread CH1` | drop-down list `Off`, `-0.5 %`, `-1.5 %`, `+/-0.5 %` |
| `Off state` | drop-down list `Low`, `High`, `High-Z` |
| Info card | Firmware, Build, Display, SI5351 |

- When the page opens, the first four rows are visible and the fifth extends past the bottom edge. The scroll bar on the right is always visible.
- The page snaps to the rows.
- Scrolling only starts after about 10 px of finger movement; a short tap remains a tap. If a swipe starts on a button, the button does not trigger, and neither does the repeat function of `Corr.`.
- Drop-down lists open upward when there is no room below.
- The scroll position is kept when you go to the main page via `MAIN` and back to CONFIG (until power-off).

### Save Indicator

The save state is shown in the title bar between `CONFIG` and `MAIN`. It is visible at every scroll position:

| Display | Meaning |
|---|---|
| `Saving...` | Change not yet saved (autosave after 5 s) |
| `Saved` | Everything saved |
| `Save failed` (red) | Saving to flash failed; a dialog also appears |
| `Not saved` (red) | Flash storage (NVS) not available at startup; changes are lost at power-off |

### Backlight

`−` and `+` change the display brightness in 10 % steps:

```text
10 % to 100 %
```

The current value is shown between the buttons. Holding a button does not repeat. The 10 % minimum prevents the display from accidentally going completely dark and thus becoming unusable. Values stored by older firmware are rounded to the nearest 10 % step; values below 5 % (e.g. 0 % from firmware 1.0.0) are set to 80 %.

### Step (Frequency Step)

The step determines how much `+` and `−` change the frequency. The same presets as on the channel pages are used:

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

### Frequency Correction (`Corr.`)

The correction compensates for the tolerance of the SI5351 crystal. It is shown in **ppm**, e.g. `+9.70 ppm`.

| Element | Function |
|---|---|
| Step button (`0.01` / `0.1` / `1`) | Step size in ppm; each tap switches to the next one |
| `−` / `+` | Change the correction by the step size; holding repeats |

Range: ±50 ppm. A positive value means that without correction, the output was too high.

The outputs are reconfigured after each change; the SI5351 is not re-initialized. In `Integer` mode, the correction cannot be applied to the output; it only shifts the displayed actual frequency (see `Tuning`).

**Calibration:**

1. Set CH1 to 10 MHz and switch it on (`Tuning` = `Fractional` or `Low noise`).
2. Measure with a frequency counter.
3. Divide the deviation in Hz by 10 = correction in ppm.
4. Add this value to the current correction.

Example:

```text
Measured: 10 000 097 Hz  ->  +97 Hz at 10 MHz  ->  set +9.70 ppm
```

### Drive all

The drive strength applies globally to all channels (CH1, CH2 and CH3); CH3 has no setting of its own.

Options:

```text
2 mA
4 mA
6 mA
8 mA
```

### Tuning (Divider Scheme)

The SI5351 generates every frequency in two stages: a PLL multiplies the 25 MHz crystal up to 600–900 MHz (VCO), and a divider (MultiSynth) divides this down to the output frequency. Both stages can work as integer or fractional (with a fractional part). Fractional dividers allow fine steps but produce spurs close to the carrier. `Tuning` determines which stage works fractionally; the setting applies to all channels.

| Option | PLL | Divider (MultiSynth) | Properties |
|---|---|---|---|
| `Fractional` (default) | fixed 900 or 887.5 MHz | fractional | finest steps, tuning without dropouts, deviation ≤ 0.5 Hz; spurs from the fractional divider possible |
| `Low noise` | fractional | even, integer | fewer spurs; short dropout when the divider changes; deviation up to ≈ 1.4 Hz |
| `Integer` | integer | even, integer | lowest spurs; frequency on a grid, deviation up to the kHz range |

Changing the mode reconfigures all outputs; running outputs drop out briefly once.

**Fractional:** Behavior as in firmware 1.2.0. The PLL stays fixed; only the divider changes. If 900 MHz cannot hit a frequency accurately enough, the PLL switches to 887.5 MHz; only then does the output drop out briefly.

**Low noise:** The divider is an even integer; the PLL handles fine tuning and frequency correction (the method used in the VFOs from QRP Labs and Etherkit). While tuning, the divider is kept as long as the PLL stays within 600–900 MHz; then only the PLL changes, without a dropout. Only when the divider has to change does the output drop out briefly. Example: divider 90 covers 6.67–10 MHz.

**Integer:** PLL and divider are both integers. The frequency can therefore only take grid values:

```text
Output = crystal × a / divider    (a = 24 … 36, divider even)
```

The firmware selects the grid point closest to the requested frequency. Example: a requested 7.1 MHz gives 25 MHz × 25 / 88 = 7.102 273 MHz.

- The frequency correction cannot be applied within the grid; it only shifts the displayed actual frequency.
- `+`/`−` change the requested frequency. The actual frequency only changes when a different grid point is closer; the `Out` line shows this.
- The main page shows the requested frequency preceded by `~` when the deviation is 1 Hz or more.
- Spread spectrum is locked.

This mode is intended for educational experiments comparing integer and fractional dividers, not for accurate frequencies.

### Spread CH1 (Spread Spectrum)

Spread spectrum (SSC) modulates the frequency of PLL A at about 31.5 kHz and spreads the carrier power over a frequency band. The SI5351 can only do this on PLL A. It therefore affects CH1, and CH3 when CH1 is its master; CH2 (PLL B) is not affected.

| Option | Effect |
|---|---|
| `Off` | off (default) |
| `-0.5 %` | Down spread: the frequency varies between the requested frequency and 0.5 % below it |
| `-1.5 %` | Down spread: between the requested frequency and 1.5 % below it |
| `+/-0.5 %` | Center spread: ±0.5 % around the requested frequency |

Example: CH1 = 10 MHz with `-0.5 %` occupies the band 9.95–10.00 MHz; the channel page shows the mean frequency `Out 9.975 000 MHz mean, SSC -0.5 %`.

- Switching resets PLL A: CH1 drops out briefly. If CH3 is running in IQ operation with master CH1, its phase is realigned afterwards.
- In `Integer` mode, the option is locked (grayed out, `Off`). Switching to `Integer` turns spread spectrum off; after switching back, it has to be selected again.
- With spread, too, the SI5351 oscillator (VCO) must stay within 600–900 MHz. With master CH1 the IQ range of CH3 therefore becomes slightly narrower: `-0.5 %` from 4.749 MHz, `-1.5 %` from 4.797 MHz, `+/-0.5 %` from 4.749 to 99.502 MHz. Outside it, CH3 waits (`WAIT`) as below 4.730 MHz. The CH3 page shows the valid range.
- For mixer and receiver experiments with a clean carrier, leave it `Off`.

### Off state (State of Switched-Off Outputs)

Determines the level that a switched-off output carries (SI5351 register 24). The setting applies to CLK0, CLK1 and CLK2.

| Option | Output in the OFF state |
|---|---|
| `Low` (default) | LOW (0 V), as up to firmware 1.2.0 |
| `High` | HIGH (supply voltage of the outputs, usually 3.3 V) |
| `High-Z` | high impedance |

- The change takes effect immediately; running outputs are not affected.
- The firmware writes the state when connecting to the SI5351, before any output can be switched on.
- **`High`:** A continuous current flows into a DC-coupled load (e.g. 50 Ω), limited by the drive strength; a DC-coupled mixer sees a DC voltage.
- For DC-coupled diode ring mixers, use `Low` or `High-Z`.
- Until hardware check K2 (measurement plan in `todo.md`), with `High` and `High-Z` the output driver of switched-off outputs stays powered, including that of CLK2 when CH3 is off. With `Low`, CLK2 is powered down completely as before, as long as CH3 is not running in IQ operation.

### Info Card

At the end of the page, a card shows information for troubleshooting without a serial monitor, for example:

```text
Firmware 1.3.0 | dev 641ebe6+
Build 2026-10-28 16:42
Display ST7789 (detected)
SI5351 SDA 22 SCL 27
```

| Line | Content |
|---|---|
| `Firmware` | Version, branch and commit the firmware was built from. `+` after the commit: built with uncommitted changes. Without git, `unknown` is shown |
| `Build` | Date and time of the build |
| `Display` | Display controller (`ILI9341` or `ST7789`) and source: `detected` (detected automatically), `stored` (stored choice from an earlier boot gesture), `boot gesture` (chosen by boot gesture at this startup), `default` (nothing detected, ILI9341 assumed), `build flag` (set at build time) |
| `SI5351` | I2C pins of the connection; suffix ` (swapped)` for swapped wires, ` (clone?)` for an unusual register 183 (e.g. a clone). Without a connection: `SI5351 not found` |

---

## 9. Saving Settings

Changes are saved automatically.

Sequence:

1. Change a setting.
2. Wait 5 seconds (on CONFIG, the indicator in the title bar changes from `Saving...` to `Saved`).
3. The firmware saves automatically to NVS.
4. The serial monitor shows:

```text
[SETTINGS] Saved to NVS
```

The following are saved:

- CH1 frequency
- CH2 frequency
- CH3 master
- CH3 phase
- Backlight
- Frequency step
- Frequency correction
- Global drive strength
- `Tuning`, `Spread CH1` and `Off state`

If the three new options are missing from storage (settings from firmware 1.2.0), the defaults `Fractional`, `Off` and `Low` apply, i.e. the behavior of 1.2.0. Invalid stored values are replaced by the default value at startup.

The display controller choice made by boot gesture is saved immediately (section 13).

Touch calibration is not saved. Default values are always used.

The ON/OFF state of the outputs is not saved; all outputs are off after power-up. Switching outputs on or off therefore does not trigger saving, nor does a setting that already has the selected value. This spares the flash memory.

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
3. Switch CH1 on.
4. Return to the main page.
5. Open CH2.
6. Enter 14 MHz.
7. Switch CH2 on.

Outputs:

```text
CLK0 = 10 MHz
CLK1 = 14 MHz
```

---

### Example C: 10 MHz IQ Output with 90°

Goal:

```text
CH1 = 10 MHz
CH3 = 10 MHz with 90° phase shift
```

Procedure:

1. Set CH1 to 10 MHz.
2. Switch CH1 on.
3. Open CH3.
4. Tap master `CH1`.
5. Tap phase `90`.
6. Switch CH3 on.

Outputs:

```text
CLK0 = 10 MHz reference
CLK2 = 10 MHz, shifted by 90°
```

---

### Example D: Changing the Output Power

Goal:

```text
All channels with 8 mA drive strength
```

Procedure:

1. Open CONFIG.
2. Select `8 mA` for `Drive all`.
3. Wait until `Saved` is shown in the title bar.

This setting then applies to all channels.

---

### Example E: Comparing Fractional and Integer Dividers

1. Set CH1 to 7.1 MHz and switch it on; connect a spectrum analyzer to CLK0.
2. In CONFIG, set `Tuning` to `Fractional`, `Low noise` and `Integer` in turn.
3. Compare the spurs. The channel page shows the actual frequency in each case; with `Integer` it shows `Out 7.102 273 MHz (+2272.7 Hz)`.

---

## 11. Errors and Notes

### Error Messages

Errors are shown as a dialog. `OK` closes the dialog. At most one dialog is open at any time; further messages appear one after another, identical messages only once. Connection messages close by themselves after reconnecting; messages about output errors stay open until they are closed with `OK`.

### SI5351 Not Found

If the SI5351 is not found at startup, **one** message appears. After that, the channels on the main page show `NO HW` and the channel pages show `SI5351 not found`. Settings can still be changed and are saved.

The firmware checks every 2 seconds whether the SI5351 responds. If it is connected later or a loose contact is fixed, the firmware reconnects automatically, restores the set frequencies and ON/OFF states and closes the connection message. If the connection is lost during operation (or the SI5351 briefly loses power), the message `Connection lost` appears and the channels show `NO HW` until the connection is restored.

SDA and SCL may be connected to CN1 in either order (GPIO22/GPIO27). The firmware tries both assignments, also after replugging during operation. The info card on CONFIG and the serial monitor show the assignment found, with the suffix `(swapped)` for swapped wires.

Further messages when connecting:

| Message | Meaning |
|---|---|
| `SI5351 not found ...` | No device at address 0x60: check the cable, 3.3 V and GND. The search continues. |
| `I2C line stays low ...` | An I2C line is permanently held at GND: short circuit, missing 3.3 V at the module or defective module. The search continues. |
| `The device at I2C address 0x60 is not an SI5351 ...` | Another device responds at 0x60. It is not programmed (at most one test byte in register 3) until it is unplugged. An SI5351 that briefly returns wrong values while being plugged in is checked again automatically. |

Note: If an SI5351 module stays powered while its cable is unplugged, its outputs keep running. On reconnection, the firmware switches them off for a few milliseconds and then restores them.

### Output Errors

If setting up an output fails although the SI5351 responds, a dialog appears. The channel shows `ERR` (main page, title bar), the channel page shows `Output error, retrying`, and the CH3 page `IQ setup failed, retrying`. The switch keeps the desired setting; the firmware retries every 2 s without another dialog. In case of an IQ error, the master keeps running.

| Message | Meaning |
|---|---|
| `Frequency set failed` | CH1 or CH2 could not be set |
| `CH3 IQ setup failed` | The IQ pair could not be set up |
| `Output switch failed` | Switching an output on or off failed |
| `Spread spectrum setup failed` | `Spread CH1` could not be written |
| `Off state setup failed` | `Off state` could not be written |

### Settings Are Not Saved

If the flash storage (NVS) is not available at startup, a dialog appears and CONFIG shows `Not saved`. If saving fails, a dialog appears and CONFIG shows `Save failed`.

### Restart After an Internal Error

If the user interface runs out of memory or the firmware hangs for more than 5 s, the device restarts by itself instead of freezing with running outputs. Afterwards, as after every power-up, all outputs are off; the stored settings are kept. Until the firmware re-initializes the SI5351, its outputs keep running briefly. The serial monitor shows the reason (section 12).

### No Output Signal

Check:

1. Is the channel set to `ON`?
2. Are you measuring the correct output?

```text
CH1 = CLK0
CH2 = CLK1
CH3 = CLK2
```

3. Is the SI5351 found on I2C? The info card on CONFIG shows the pins (`SI5351 SDA 22 SCL 27`) or `SI5351 not found`. The serial monitor should show:

```text
[APP] SI5351 at 0x60, SDA=22 SCL=27
```

4. Is there a common ground?
5. Is the SI5351 supplied with 3.3 V?
6. If the SI5351 module has an OE pin, is it at the active level?

Many SI5351 modules have an active-low OE:

```text
OE = GND -> outputs enabled
OE = HIGH -> outputs disabled
```

### CH3 Shows WAIT or "Waiting"

Possible causes:

```text
The master is switched off.
The master frequency is below 4.730 MHz.
```

Solution:

- switch the master on
- set the master frequency to at least 4.730 MHz

CH3 then starts automatically; the switch does not need to be operated again.

### Limitations

- Touch calibration is fixed and cannot be changed.
- The displayed actual frequency assumes that the frequency correction is right.
- IQ operation only from a master frequency of 4.730 MHz.

---

## 12. Serial Diagnostics

Serial monitor:

```bash
pio device monitor -b 115200
```

Important messages:

| Message | Meaning |
|---|---|
| `[BOOT] CYD HF Generator 1.3.0 (dev 641ebe6+, 2026-10-28 16:42)` | Startup: version, branch, commit and build time as on the info card |
| `[I2C] Line held low; releasing bus` | An I2C line was LOW at startup and was released |
| `[SETTINGS] Loaded from NVS` | Settings loaded |
| `[SI5351] Init OK; all outputs off` | SI5351 initialized, all outputs off |
| `[APP] SI5351 at 0x60, SDA=22 SCL=27` | Connected; with swapped wires `SDA=27 SCL=22 (swapped)` |
| `[APP] SI5351 not found` | No SI5351 found; the search continues |
| `[TOUCH] XPT2046 init OK on VSPI` | Touch ready |
| `[DISPLAY] ST7789 (detected); ...` | Display controller, source and detection readings |
| `[BOOT] Setup complete` | Startup complete |
| `[SETTINGS] Saved to NVS` | Autosave |
| `[APP] Restoring outputs` | After reconnecting, the outputs are being restored |
| `[APP] Output error cleared` | An output error (`ERR`) has been cleared |

Error messages start with `[ERROR]`, e.g.:

| Message | Meaning |
|---|---|
| `[ERROR][SI5351] Device lost or reset` | Supervision: the SI5351 does not respond or has lost its configuration |
| `[ERROR][SI5351] Unexpected register 183 (clone?); continuing` | Unusual register 183, e.g. a clone; the module is used anyway |
| `[ERROR][APP] I2C fails at 400 kHz; staying at 100 kHz` | The bus only works at 100 kHz (e.g. weak pull-ups) |
| `[ERROR][SETTINGS] Saving to NVS failed` | Saving failed |
| `[ERROR][FATAL] LVGL assert; restarting` | User interface out of memory or similar; the device restarts |
| `task_wdt: Task watchdog got triggered … loopTask` | The firmware hung for 5 s; the device restarts |

Dialog texts also appear in the log, e.g. `[ERROR][SI5351] Frequency set failed`.

Frequency changes and detailed SI5351 messages (output mask, phase register, I2C scan, etc.) only appear if the firmware was built with `LOG_DEBUG 1` (in `include/config.h`). They are off in normal operation because serial output slows down the user interface.

---

## 13. Current Hardware Pinout

The SI5351 is connected to CN1. SDA and SCL may be swapped; the firmware detects the assignment:

```text
SDA / SCL = GPIO22 / GPIO27 (either order; tried first: SDA = GPIO22)
SI5351 address = 0x60
```

Display and touch use separate SPI buses. The complete pinout is given in [PROJECT_SPECIFICATION.md](PROJECT_SPECIFICATION.md), section 2.

### Board Variants

Two boards are sold under the name ESP32-2432S028R; they differ only in the display controller. The same firmware runs on both; it detects the controller at power-on:

| Board | Distinguishing feature | Display controller |
|---|---|---|
| Original CYD | micro-USB port only | ILI9341 |
| CYD2USB | additional USB-C port | ST7789 |

The info card on CONFIG and the serial monitor show the result, for example `Display ST7789 (detected)` and `[DISPLAY] ST7789 (detected); ...` respectively. Pinout, touch and operation are the same on both variants.

**Display shows only noise (boot gesture):** Touch the display, switch the device on and keep touching until the backlight blinks twice, then let go. The firmware then uses the other controller and remembers the choice. A second boot gesture switches back to automatic detection. The gesture only works if the touch is already present at power-on and is held for 1.5 s. A touch at power-on is not passed on to the user interface as a tap.

---

## 14. Safety Note

The SI5351 outputs deliver digital square-wave signals. Before connecting them to sensitive HF circuits, check:

- signal level
- load impedance
- possible attenuation
- galvanic connection
- ground routing
- state of switched-off outputs (`Off state`): with `High`, a switched-off output carries a DC voltage
