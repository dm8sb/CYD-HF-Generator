# Hardware

What you need to build the CYD HF Generator and how to connect the SI5351 module. Operation, messages and limits are described in the user manual ([English](../USER_MANUAL_EN.md), [German](../USER_MANUAL_DE.md)); the authoritative pin table is in the [project specification, section 2](../PROJECT_SPECIFICATION.md#2-hardware-and-pinout).

<!-- Photo: complete setup on the bench (see "Photos" below)
![CYD HF Generator on the bench](img/setup-bench.jpg)
-->

## Contents

- [Bill of materials](#bill-of-materials)
- [Board variants](#board-variants)
- [Wiring the SI5351 to CN1](#wiring-the-si5351-to-cn1)
- [Supply](#supply)
- [Outputs](#outputs)
- [Hints for a clean signal](#hints-for-a-clean-signal)
- [Case](#case)
- [Safety](#safety)
- [Photos](#photos)

## Bill of materials

| Item | Qty | Notes |
|---|---:|---|
| ESP32-2432S028R ("Cheap Yellow Display", 2.8" 320×240) | 1 | Original CYD (micro-USB only, ILI9341) or CYD2USB (additional USB-C port, ST7789). The same firmware runs on both. |
| SI5351A breakout module | 1 | 25 MHz crystal, I2C address 0x60, runs on 3.3 V (for example Adafruit 2045 or one of the many clone boards). Clones such as the MS5351M work; the firmware notes them in the log. |
| Cable for CN1 | 1 | 4-pin cable for the CN1 connector (1.25 mm pitch; a cable often comes with the board) with Dupont ends for the module, 4 wires: 3.3 V, GND, GPIO22, GPIO27. Keep it short. |
| USB cable and 5 V supply | 1 | Micro-USB (both boards) or USB-C (CYD2USB), USB power supply 5 V / 1 A or a PC port. |
| SMA connectors and coax pigtails | optional, up to 3 | Many SI5351 modules have footprints for SMA edge connectors on CLK0–CLK2. |
| 50 Ω termination, attenuator, DC block | optional | For measurements: a DC block in front of DC-coupled inputs, 20 dB in front of a spectrum analyzer. |
| Low-pass filter per output | optional | Required before a square-wave output goes anywhere near an antenna (see [Safety](#safety)). |
| Low-noise 3.3 V LDO with ferrite bead and capacitors | optional | Separate supply for the SI5351 module (see [Hints](#hints-for-a-clean-signal)). |
| Metal enclosure or shielding | optional | Keeps the display and ESP32 noise away from the outputs. |
| 25 MHz TCXO | optional | Only if the warm-up drift turns out to matter; this depends on a pending measurement (MESSPLAN C7). |

## Board variants

Both variants are sold as ESP32-2432S028R. They differ only in the display controller; pinout, touch, backlight and CN1 are identical, and the firmware detects the controller at every boot (manual [section 13](../USER_MANUAL_EN.md#13-current-hardware-pinout)).

| Board | Distinguishing feature | Display controller |
|---|---|---|
| Original CYD | micro-USB port only | ILI9341 |
| CYD2USB | additional USB-C port | ST7789 |

<!-- Photos: front of both boards with the MAIN page
![Original CYD with the MAIN page](img/board-cyd-front.jpg)
![CYD2USB with the MAIN page](img/board-cyd2usb-front.jpg)
-->

## Wiring the SI5351 to CN1

CN1 is the 4-pin connector on the board that carries 3.3 V, GND, GPIO22 and GPIO27. Follow the labels printed next to CN1, not the wire colors of a supplied cable.

| CN1 pin | SI5351 module pin | Notes |
|---|---|---|
| 3.3V | VIN / VCC / 3V3 | Supply of the module (see [Supply](#supply)). |
| GND | GND | Common ground; also the reference for the outputs. |
| IO22 | SDA | Tried first as SDA (verified wiring). |
| IO27 | SCL | |

**SDA and SCL may be swapped.** The firmware tries both assignments at boot and every 2 s while no SI5351 is connected, so IO22/IO27 can go to SDA/SCL in either order, and the module can be plugged in while the device is running. The CONFIG info card and the serial log show the assignment found, with `(swapped)` for swapped wires (manual [section 11](../USER_MANUAL_EN.md#11-errors-and-notes)).

```text
CN1 3.3V ---- VIN   SI5351 module
CN1 GND  ---- GND
CN1 IO22 ---- SDA   (or SCL, the firmware detects it)
CN1 IO27 ---- SCL   (or SDA)
                    CLK0 -> CH1
                    CLK1 -> CH2
                    CLK2 -> CH3 (IQ follower)
```

- I2C address 0x60. Do not connect other devices with this address to CN1.
- Pull-ups: most modules have them on board. The firmware detects the chip at 100 kHz and switches to 400 kHz only if the bus works there; with weak pull-ups it stays at 100 kHz.
- If the module has an `OE` pin: on most modules it is active low, `OE` = GND enables the outputs. Check whether the module pulls it low by itself; otherwise connect it to GND.
- GPIO21 (backlight, also on connector P3) is never used for I2C.

<!-- Photos: CN1 connector and SI5351 wiring close-up
![CN1 connector with labels](img/cn1-connector.jpg)
![SI5351 module wired to CN1](img/si5351-wiring.jpg)
-->

## Supply

- The board is powered over USB (5 V). The SI5351 module takes its 3.3 V from CN1, which comes from the regulator on the board.
- Modules with their own regulator accept 3.3 V on their VIN pin; check the module's documentation. A 5 V supply on a module without a regulator destroys the SI5351.
- The SI5351 keeps running with its last setting while the ESP32 is reset or flashed, and as long as the module stays powered with the cable unplugged. Disconnect sensitive loads before flashing.
- For the lowest spurs, supply the module from its own low-noise 3.3 V LDO (see [Hints](#hints-for-a-clean-signal)); keep the grounds connected.

## Outputs

| SI5351 output | Channel | Function |
|---|---|---|
| CLK0 | CH1 | Freely adjustable, 8 kHz–160 MHz |
| CLK1 | CH2 | Freely adjustable, 8 kHz–160 MHz |
| CLK2 | CH3 | IQ follower of CH1 or CH2, 0/90/180/270°, 4.730–100 MHz |

- The outputs are CMOS square waves between GND and the output supply of the module (usually 3.3 V), with a drive strength of 2–8 mA (CONFIG `Drive all`). Level and duty cycle into 50 Ω are not measured yet (MESSPLAN E5, F2).
- A switched-off output is Low, High or High-Z (CONFIG `Off state`). With `High`, it carries a DC voltage: use a DC block in front of sensitive inputs.
- For IQ measurements use coax cables of equal length from CLK0/CLK1 and CLK2; 1 mm of cable is about 5 ps.

## Hints for a clean signal

These hints are general practice for SI5351 builds; their effect on this device is still being measured (MESSPLAN F6, F9, C7).

- **Own supply for the module:** a low-noise 3.3 V LDO for the SI5351, fed through a ferrite bead, with 100 nF and 10 µF close to the module, keeps the ESP32, WiFi and display noise out of the outputs.
- **Short ground:** a short, wide ground connection between board and module; coax shields to the module ground right at the output.
- **Short I2C cable:** keep the CN1 cable short and away from the output lines.
- **Shielding:** a metal enclosure or a shield over the module reduces coupling from the display, the backlight PWM and the WiFi radio.
- **TCXO:** an optional 25 MHz TCXO instead of the crystal, only if the warm-up drift matters for your use (decision after measurement C7). The frequency correction (CONFIG `Corr.`) compensates the static crystal error.

## Case

[`case/CYD_Si5351_Gehaeuse.scad`](../case/CYD_Si5351_Gehaeuse.scad) is a parametric OpenSCAD case for the CYD2USB with a CJMCU-5351 module and three edge-mount SMA sockets (outside about 90.6 × 57 × 32.3 mm, two parts, printable without support, snap hooks or M2.5 screws). Measure your Si5351 module first; parameters, assembly and STL export: [case/README.md](../case/README.md).

## Safety

- **Never connect an output to an antenna without a filter.** The outputs are square waves rich in harmonics up to several hundred MHz. Transmitting requires a license and a low-pass filter that meets the spurious emission limits; for receiver and mixer experiments use cables, attenuators and terminations, not an antenna.
- Do not connect an output to the antenna or output port of a transmitter, or to anything that can feed power or DC back into the SI5351.
- Before connecting sensitive HF circuits, check signal level, load impedance, attenuation, galvanic connection, ground routing and the `Off state` (manual [section 14](../USER_MANUAL_EN.md#14-safety-note)).
- Physical access is full access: anyone who can operate the device or reflash it over USB can change all settings and switch outputs on.

## Photos

The images below are still to be taken. Save them under `docs/img/` with these names, then remove the comment markers around the image links in this file.

| File | Content | Used in |
|---|---|---|
| `docs/img/board-cyd-front.jpg` | Original CYD, front, MAIN page with CH1 on, display sharp and free of reflections | [Board variants](#board-variants) |
| `docs/img/board-cyd2usb-front.jpg` | CYD2USB, front, same MAIN page; the USB-C port visible | [Board variants](#board-variants) |
| `docs/img/si5351-wiring.jpg` | Close-up of the SI5351 module with the four wires to CN1 and the CLK0–CLK2 outputs, labels readable | [Wiring](#wiring-the-si5351-to-cn1) |
| `docs/img/cn1-connector.jpg` | Back of the board, CN1 connector with its silkscreen labels readable | [Wiring](#wiring-the-si5351-to-cn1) |
| `docs/img/setup-bench.jpg` | Complete setup on the bench: board, module, coax to an oscilloscope or spectrum analyzer | top of this page, README |
| `docs/img/social-preview.png` | Social preview for the repository, 1280×640 px (at least 640×320, under 1 MB), for example the bench photo with the project name; uploaded in the repository settings, not linked here | GitHub settings |

Keep the photos under about 500 KB each (JPEG, about 1600 px wide) so the repository stays small, and make sure no WiFi names or passwords are visible on the display.
