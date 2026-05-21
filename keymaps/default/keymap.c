#include QMK_KEYBOARD_H
#include "analog.h"
#include "gpio.h"
#include "joystick.h"

static uint16_t joy_center_x = 512;
static uint16_t joy_center_y = 512;

static int16_t clamp_axis(int32_t v) {
    if (v > 127) {
        return 127;
    }
    if (v < -127) {
        return -127;
    }
    return (int16_t)v;
}

static int16_t normalize_axis(uint16_t raw, uint16_t center) {
    int32_t delta = (int32_t)raw - (int32_t)center;

    if (delta > -JOYSTICK_DEADZONE && delta < JOYSTICK_DEADZONE) {
        return 0;
    }

    // Scale from around 10-bit ADC space to HID joystick range.
    return clamp_axis(delta / 4);
}

void keyboard_post_init_user(void) {
    gpio_set_pin_input_high(JOYSTICK_SW_PIN);

    // Read center once after power-up while stick is untouched.
    joy_center_x = analogReadPin(JOYSTICK_X_PIN);
    joy_center_y = analogReadPin(JOYSTICK_Y_PIN);
}

void housekeeping_task_user(void) {
    uint16_t raw_x = analogReadPin(JOYSTICK_X_PIN);
    uint16_t raw_y = analogReadPin(JOYSTICK_Y_PIN);

    int16_t axis_x = normalize_axis(raw_x, joy_center_x);
    int16_t axis_y = normalize_axis(raw_y, joy_center_y);

    // Typical gamepad convention: up is negative Y.
    axis_y = -axis_y;

    joystick_set_axis(0, axis_x);
    joystick_set_axis(1, axis_y);

    if (!gpio_read_pin(JOYSTICK_SW_PIN)) {
        register_joystick_button(0);
    } else {
        unregister_joystick_button(0);
    }
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(KC_ESC)
};
