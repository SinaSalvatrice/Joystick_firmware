#pragma once

#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 200U

// RP2040 analog inputs are limited to GP26..GP29.
// GP0 and GP1 are digital-only and cannot read analog joystick axes.
#define JOYSTICK_X_PIN GP26
#define JOYSTICK_Y_PIN GP27

// GP2 and GP3 are the layer buttons. GP4 is the stick click.
// The direct-pin order is configured in keyboard.json.

// ADC values are 10-bit by default in QMK (0..1023).
#define JOYSTICK_DEADZONE 24
#define JOYSTICK_DIRECTION_THRESHOLD 48
