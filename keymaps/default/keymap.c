#include QMK_KEYBOARD_H
#include "analog.h"
#include "gpio.h"
#ifdef OLED_ENABLE
#    include "oled_driver.h"
#endif

static uint16_t joy_center_x = 512;
static uint16_t joy_center_y = 512;
static int16_t  last_axis_x   = 0;
static int16_t  last_axis_y   = 0;
static bool     stick_pressed = false;

static bool left_pressed  = false;
static bool right_pressed = false;
static bool up_pressed    = false;
static bool down_pressed  = false;
static bool enter_pressed = false;

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

    // Scale from the ADC range to a compact signed value for threshold checks.
    return clamp_axis(delta / 4);
}

static void sync_key_state(uint16_t keycode, bool *was_pressed, bool is_pressed) {
    if (is_pressed == *was_pressed) {
        return;
    }

    if (is_pressed) {
        register_code(keycode);
    } else {
        unregister_code(keycode);
    }

    *was_pressed = is_pressed;
}

static const char *direction_name(int16_t axis_x, int16_t axis_y) {
    bool left  = axis_x < -direction_threshold;
    bool right = axis_x > direction_threshold;
    bool up    = axis_y < -direction_threshold;
    bool down  = axis_y > direction_threshold;

    if (up && left) {
        return "UL";
    }
    if (up && right) {
        return "UR";
    }
    if (down && left) {
        return "DL";
    }
    if (down && right) {
        return "DR";
    }
    if (up) {
        return "U";
    }
    if (down) {
        return "D";
    }
    if (left) {
        return "L";
    }
    if (right) {
        return "R";
    }

    return "C";
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

    sync_key_state(KC_LEFT, &left_pressed, axis_x < -direction_threshold);
    sync_key_state(KC_RIGHT, &right_pressed, axis_x > direction_threshold);
    sync_key_state(KC_UP, &up_pressed, axis_y < -direction_threshold);
    sync_key_state(KC_DOWN, &down_pressed, axis_y > direction_threshold);

    stick_pressed = !gpio_read_pin(JOYSTICK_SW_PIN);
    sync_key_state(KC_ENT, &enter_pressed, stick_pressed);
}

#ifdef OLED_ENABLE
bool oled_task_user(void) {
    oled_clear();
    oled_write_ln_P(PSTR("ARROWS"), false);
    oled_write_ln(direction_name(last_axis_x, last_axis_y), false);
    oled_write_ln_P(PSTR("BTN ENT"), false);
    oled_write_ln_P(stick_pressed ? PSTR("PUSH") : PSTR("OPEN"), false);
    return false;
}
#endif

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(JS_1)
};
