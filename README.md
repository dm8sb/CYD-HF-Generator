# CYD HF Generator

PlatformIO + Arduino-ESP32 firmware for an ESP32-2432S028R / Sunton Cheap Yellow Display based 3-channel HF square-wave generator using an SI5351.

## User Manuals

- German: [USER_MANUAL_DE.md](USER_MANUAL_DE.md)
- English: [USER_MANUAL_EN.md](USER_MANUAL_EN.md)

## Features

- ILI9341 320x240 landscape GUI with LVGL
- XPT2046 touch on a separate VSPI bus
- SI5351 clock generator on explicit I2C pins SDA=27 / SCL=22
- CH1 and CH2 independent 1-100 MHz outputs
- CH3 IQ follower of CH1 or CH2 with selectable 0/90/180/270 degree phase setting
- Independent output enable/High-Z control
- 2/4/6/8 mA drive strength selection
- Backlight PWM on GPIO21 only
- Preferences/NVS persistence with 5 second autosave debounce
- Serial logging for boot, I2C, SI5351, touch, frequency, output and settings events

## Pinout

| Function | Bus | GPIO |
|---|---:|---:|
| TFT MOSI | HSPI | 13 |
| TFT MISO | HSPI | 12 |
| TFT SCK | HSPI | 14 |
| TFT CS | HSPI | 15 |
| TFT DC | - | 2 |
| TFT RST | - | -1 / EN |
| Touch MOSI | VSPI | 32 |
| Touch MISO | VSPI | 39 |
| Touch SCK | VSPI | 25 |
| Touch CS | VSPI | 33 |
| Touch IRQ | - | 36 |
| SI5351 SDA | I2C | 22 |
| SI5351 SCL | I2C | 27 |
| Backlight PWM | LEDC | 21 |
| RGB LED R | active low | 4 |
| RGB LED G | active low | 16 |
| RGB LED B | active low | 17 |
| Free input only | - | 35 |


Note: the original hardware specification listed SDA=27/SCL=22. This project is now set to the wiring that was verified on the actual device by I2C scan: SDA=22/SCL=27, SI5351 at address 0x60.

## Libraries

Pinned or requested by `platformio.ini`:

- Arduino-ESP32 via `espressif32@6.7.0`
- `bodmer/TFT_eSPI@^2.5.43`
- `lvgl/lvgl@8.3.11`
- `paulstoffregen/XPT2046_Touchscreen@1.4`
- `https://github.com/osmanovv/esp32-si5351.git`
- `Preferences` from Arduino-ESP32

## Build and Flash

```bash
cd cyd_hf_generator
pio run
pio run -t upload
pio device monitor -b 115200
```

The project uses `huge_app.csv`. A standard 4 MB ESP32 module is sufficient.

## Operation

- Main page shows all channel states.
- CH1/CH2 pages allow +/- step adjustment, ON/OFF, and numpad entry.
- Numpad accepts max 9 characters and units Hz/kHz/MHz. If OK is used, Hz is assumed.
- CH3 page selects master, phase, drive and follower enable.
- Config page sets backlight, delta step, SI5351 correction and one global drive strength for all channels.

## Notes and Limitations

- CH3 is always configured with the same PLL as its master through `si5351_CalcIQ()`.
- The selected SI5351 library exposes no direct PLL lock readback; the firmware logs that limitation and continues safely.
- The SI5351 phase register is 7-bit. Some 180/270 degree requests at low frequencies can exceed that hardware/library phase-offset field; the code logs this and applies the masked register value.
- Touch calibration UI and runtime calibration code are disabled. The firmware always uses the built-in default XPT2046 calibration values: x_min=300, x_max=3800, y_min=300, y_max=3800.
- Drive strength is a single global CONFIG value for CH1, CH2 and CH3; CH3 has no independent drive setting.
- The firmware validates CH3 phase offsets against the SI5351 7-bit phase register and rejects impossible phase/frequency combinations.
- Switching artifacts are allowed by design. The firmware disables outputs before reconfiguration and restores only enabled channels.

## Architecture

The project follows a modular structure:

- `main.cpp`: initialization order and loop
- `core/app_controller.*`: single source of truth for runtime state and SI5351 coordination
- `drivers/*`: SI5351, touch and backlight hardware drivers
- `services/*`: Preferences storage and autosave
- `hal/i2c_bus.*`: explicit I2C setup
- `gui/gui.cpp`: LVGL setup, navigation, shared widgets and callbacks
- `gui/pages/*`: page rendering for Main, channel, CH3, Config and Numpad pages
- `utils/*`: logging and frequency helpers


