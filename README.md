# Joystick_firmware

QMK keyboard firmware for a two-axis analog joystick and three buttons on an **RP2040**.

## Wiring (RP2040)

| Input | RP2040 pin | Function |
| --- | --- | --- |
| Joystick axis X | GP26 / ADC0 | Left / right |
| Joystick axis Y | GP27 / ADC1 | Up / down |
| Joystick click | GP2 -> GND | Select |
| Button 1 | GP3 -> GND | Momentary Layer 1 |
| Button 2 | GP4 -> GND | Momentary Layer 2 |

**Important:** RP2040 GPIO **GP0 and GP1 are not ADC pins**. Analog joystick
outputs wired there **cannot** be read by this QMK/RP2040 firmware. Move the
analog signal wires to GP26 and GP27 (or use an external ADC). Switches use
internal pull-ups, so connect each button between its GPIO pin and GND.

There is no OLED, LED strip, or other peripheral in the QMK configuration.
The separate `Ino/joystick.ino` is an older ESP32-C3 example, not part of
the QMK build.

## Default actions

| Stick action | Base layer | Hold GP3 (Layer 1) | Hold GP4 (Layer 2) |
| --- | --- | --- | --- |
| Up | Arrow Up | W | Page Up |
| Down | Arrow Down | S | Page Down |
| Left | Arrow Left | A | Home |
| Right | Arrow Right | D | End |
| Press GP2 | Enter | Space | Escape |

GP3 and GP4 use QMK `MO(1)` / `MO(2)`: the corresponding layer stays
active only while the button is held. If both are held, Layer 2 wins.

## Local build (Windows)

1. Install QMK with an RP2040-capable ARM toolchain.
2. Set `QMK_HOME` to the `qmk_firmware` checkout, or place it in one of the
   common locations searched by `build.bat`.
3. Run `build.bat` on Windows. It uses `qmk` on PATH, or automatically
   falls back to QMK MSYS installed at `C:\\QMK_MSYS` when available.
   The script copies the keyboard files into
   `qmk_firmware/keyboards/joystick_pad` and runs
   `qmk compile -kb joystick_pad -km default`.
4. If successful, the generated `.uf2` is copied into `artifacts/`.

No GitHub Actions build is configured. Double-tap reset on RP2040 to enter
the USB bootloader, then copy the .uf2 to the RPI-RP2 drive.

Boot-up auto-calibration: do not touch the stick while the board starts.
