#include QMK_KEYBOARD_H
#include "analog.h"
#include "gpio.h"
#include "joystick.h"
#ifdef OLED_ENABLE
#    include "oled_driver.h"
#endif

static uint16_t joy_center_x = 512;
static uint16_t joy_center_y = 512;
static int16_t  last_axis_x   = 0;
static int16_t  last_axis_y   = 0;
static bool     stick_pressed = false;

joystick_config_t joystick_axes[JOYSTICK_AXIS_COUNT] = {
    JOYSTICK_AXIS_VIRTUAL,
    JOYSTICK_AXIS_VIRTUAL,
};

static const int16_t direction_threshold = 48;

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

static const char *direction_name(int16_t axis_x, int16_t axis_y) {
    bool left  = axis_x < -direction_threshold;
    bool right = axis_x > direction_threshold;
    bool up    = axis_y < -direction_threshold;
    bool down  = axis_y > direction_threshold;

    if (up && left) {
        return "UP-LEFT";
    }
    if (up && right) {
        return "UP-RIGHT";
    }
    if (down && left) {
        return "DOWN-LEFT";
    }
    if (down && right) {
        return "DOWN-RIGHT";
    }
    if (up) {
        return "UP";
    }
    if (down) {
        return "DOWN";
    }
    if (left) {
        return "LEFT";
    }
    if (right) {
        return "RIGHT";
    }

    return "CENTER";
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

    last_axis_x = axis_x;
    last_axis_y = axis_y;

    joystick_set_axis(0, axis_x);
    joystick_set_axis(1, axis_y);

    stick_pressed = !gpio_read_pin(JOYSTICK_SW_PIN);

    if (stick_pressed) {
        register_joystick_button(0);
    } else {
        unregister_joystick_button(0);
    }
}

#ifdef OLED_ENABLE
bool oled_task_user(void) {
    oled_clear();
    oled_write_ln_P(PSTR("JOYSTICK"), false);
    oled_write_ln(direction_name(last_axis_x, last_axis_y), false);
    oled_write_P(PSTR("SW:"), false);
    oled_write_ln_P(stick_pressed ? PSTR("PUSH") : PSTR("OPEN"), false);
    oled_write_ln_P(PSTR("KEY: JS1"), false);
    return false;
}
#endif

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(JS_1)
};
