# User Manual – CYD HF Generator

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

CH3 always follows the frequency of its selected master and can use a phase offset of 0°, 90°, 180° or 270°, if the selected combination is valid for the SI5351.

---

## 2. Basic Operation

After power-up, the main page is displayed.

The main page shows:

- CH1 frequency and ON/OFF state
- CH2 frequency and ON/OFF state
- CH3 frequency, master, phase and ON/OFF state
- Navigation buttons for CH1, CH2, CH3 and CONFIG

The navigation buttons are color-coded:

| Button | Color |
|---|---|
| CH1 | Red |
| CH2 | Green |
| CH3 | Blue |
| CONFIG | Yellow |

The RGB LED also indicates the active channel page:

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

This means:

| Display | Frequency |
|---|---:|
| 1.000 000 MHz | 1 MHz |
| 5.000 000 MHz | 5 MHz |
| 10.700 000 MHz | 10.7 MHz |
| 100.000 000 MHz | 100 MHz |

---

## 4. Using CH1 and CH2

CH1 and CH2 are freely adjustable frequency outputs.

### Turning CH1 or CH2 ON

1. On the main page, press `CH1` or `CH2`.
2. Turn on the ON/OFF switch in the title bar.
3. The display changes to `ON`.
4. A signal is available at the corresponding SI5351 output:

| Channel | SI5351 Output |
|---|---|
| CH1 | CLK0 |
| CH2 | CLK1 |

### Turning CH1 or CH2 OFF

1. Open the channel page.
2. Turn off the ON/OFF switch.
3. The display changes to `OFF`.
4. The output is disabled.

---

## 5. Changing Frequency with + and -

On CH1 and CH2 pages there are three buttons:

```text
+
-
NUMPAD
```

The `+` and `-` buttons change the frequency by the currently configured delta step.

The delta step is configured on the CONFIG page.

Example:

```text
Current frequency: 10.000 000 MHz
Delta: 1 000 Hz
Press +
New frequency: 10.001 000 MHz
```

Another example:

```text
Current frequency: 7.100 000 MHz
Delta: 10 000 Hz
Press -
New frequency: 7.090 000 MHz
```

---

## 6. Entering Frequency with the NUMPAD

On CH1 or CH2:

1. Press `NUMPAD`.
2. Enter the number.
3. Optionally use a decimal point.
4. Press one unit button:
   - `Hz`
   - `kHz`
   - `MHz`

The unit button confirms the entry immediately.

There is no separate OK button.

### Numpad Buttons

| Button | Function |
|---|---|
| 0–9 | Numeric input |
| . | Decimal point |
| `<--` | Delete last character |
| Clear | Clear input |
| Hz | Apply value as Hz |
| kHz | Apply value as kHz |
| MHz | Apply value as MHz |

The input is limited to 9 characters.

### Example 1: Enter 10 MHz

1. Open CH1.
2. Press `NUMPAD`.
3. Enter:

```text
10
```

4. Press `MHz`.
5. CH1 is set to approximately 10 MHz.

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

CH3 is not an independently adjustable frequency channel. CH3 follows CH1 or CH2.

On the CH3 page you can select:

- Master: CH1 or CH2
- Phase: 0°, 90°, 180° or 270°
- ON/OFF

CH3 always uses the same frequency as its master.

### Example: CH3 follows CH1 with 90° phase

1. Set CH1 to 10 MHz.
2. Turn CH1 ON.
3. Open the CH3 page.
4. Set Master to `CH1`.
5. Set Phase to `90`.
6. Turn CH3 ON.

Result:

```text
CH1 / CLK0 = 10 MHz, 0° reference
CH3 / CLK2 = 10 MHz, 90° phase shifted
```

### Example: CH3 follows CH2 with 180° phase

1. Set CH2 to 5 MHz.
2. Turn CH2 ON.
3. Open CH3.
4. Set Master to `CH2`.
5. Set Phase to `180`.
6. Turn CH3 ON.

Result:

```text
CH2 / CLK1 = 5 MHz
CH3 / CLK2 = 5 MHz, 180° phase shifted
```

Note: Not every phase is technically possible at every frequency. If the SI5351 phase register limit is exceeded, the device shows an error message and safely disables CH3.

---

## 8. CONFIG Page

The CONFIG page contains global settings.

### Backlight

The slider controls the display brightness:

```text
0 % to 100 %
```

### Delta Step

The delta step defines how much `+` and `-` change the frequency.

Buttons:

| Button | Function |
|---|---|
| x10 | Multiply delta step by ten |
| /10 | Divide delta step by ten |

Example:

```text
Delta: 1 000 Hz
Press x10
Delta: 10 000 Hz
```

### Frequency Correction

The `+10` and `-10` buttons adjust the SI5351 frequency correction value.

This setting is used to calibrate the SI5351 crystal.

Example:

If a frequency counter shows an error at 10 MHz, adjust the correction until the measured frequency is as close as possible to the target value.

### Drive all

Drive strength is global for all channels.

Available values:

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

CH3 has no separate drive-strength setting.

---

## 9. Saving Settings

Settings are saved automatically.

Process:

1. Change a setting.
2. Wait 5 seconds.
3. The firmware stores the settings in NVS.
4. The Serial Monitor shows:

```text
[SETTINGS] Saved to NVS
```

Saved settings include:

- CH1 frequency
- CH2 frequency
- CH1 ON/OFF
- CH2 ON/OFF
- CH3 ON/OFF
- CH3 master
- CH3 phase
- Backlight
- Delta step
- Frequency correction
- Global drive strength

Touch calibration is not saved. The firmware always uses default values.

---

## 10. Usage Examples

### Example A: Simple 10 MHz Output on CH1

1. Open the main page.
2. Press `CH1`.
3. Press `NUMPAD`.
4. Enter `10`.
5. Press `MHz`.
6. Turn CH1 `ON`.

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
4. Set Master = CH1.
5. Set Phase = 90.
6. Turn CH3 ON.

Outputs:

```text
CLK0 = 10 MHz reference
CLK2 = 10 MHz, 90° shifted
```

---

### Example D: Change Output Drive Strength

Goal:

```text
All channels use 8 mA drive strength
```

Procedure:

1. Open CONFIG.
2. Select `8 mA` under `Drive all`.
3. Wait 5 seconds for autosave.

This setting now applies to all channels.

---

## 11. Troubleshooting

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

The Serial Monitor should show:

```text
[I2C] Found device at 0x60
```

4. Is there a common ground?
5. Is the SI5351 powered from 3.3 V?
6. If your SI5351 module has an OE pin, is it enabled?

Many SI5351 modules use active-low OE:

```text
OE = GND -> outputs enabled
OE = HIGH -> outputs disabled
```

### CH3 Cannot Be Enabled with a Selected Phase

Possible reason:

```text
The requested phase is not representable in the SI5351 phase register at this frequency.
```

Solution:

- choose another phase
- choose another frequency
- turn CH3 ON again

---

## 12. Serial Diagnostics

Open the Serial Monitor:

```bash
pio device monitor -b 115200
```

Important messages:

```text
[I2C] Found device at 0x60
[SI5351] Init OK
[SI5351] Set Freq CLK0: ...
[SI5351] Output mask: ...
[SETTINGS] Saved to NVS
```

---

## 13. Current Hardware Pinout

For this project, the SI5351 I2C wiring was verified by I2C scan:

```text
SDA = GPIO22
SCL = GPIO27
SI5351 address = 0x60
```

Display and touch use separate SPI buses.

---

## 14. Safety Note

The SI5351 outputs are digital square-wave outputs. Before connecting them to sensitive RF circuitry, check:

- signal level
- load impedance
- attenuation requirements
- galvanic connection
- grounding

