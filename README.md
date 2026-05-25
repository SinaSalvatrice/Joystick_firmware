# Joystick_firmware

QMK firmware for a single-stick joystick pad targeting RP2040.

## Build output

This repo is configured for RP2040 and GitHub Actions builds should produce a
`.uf2` firmware artifact.

## Default RP2040 pin mapping

The current configuration uses these default GPIO assignments:

- Joystick X axis: `GP28`
- Joystick Y axis: `GP29`
- Joystick switch / matrix input: `GP16`
- OLED I2C SDA: `GP0`
- OLED I2C SCL: `GP1`
- WS2812 LED strip data: `GP7` (18 LEDs)

If your board is wired differently, update [config.h](config.h) and
[keyboard.json](keyboard.json) to match your actual GPIO mapping.
