#include QMK_KEYBOARD_H
#include "analog.h"

static uint16_t center_x = 512;
static uint16_t center_y = 512;

// Track the actual keycodes, so switching layers while holding the stick
// releases the old keys instead of leaving them stuck.
typedef struct {
    uint16_t keycode;
    bool     pressed;
} held_key_t;

static held_key_t left_key  = {KC_NO, false};
static held_key_t right_key = {KC_NO, false};
static held_key_t up_key    = {KC_NO, false};
static held_key_t down_key  = {KC_NO, false};

static int16_t axis_value(uint16_t raw, uint16_t center) {
    int32_t delta = (int32_t)raw - (int32_t)center;

    if (delta > -JOYSTICK_DEADZONE && delta < JOYSTICK_DEADZONE) {
        return 0;
    }

    int32_t value = delta / 4;
    if (value > 127) {
        value = 127;
    } else if (value < -127) {
        value = -127;
    }
    return (int16_t)value;
}

static void update_held_key(held_key_t *key, uint16_t desired, bool active) {
    if (key->pressed && (!active || key->keycode != desired)) {
        unregister_code16(key->keycode);
        key->pressed = false;
    }
    if (active && !key->pressed) {
        register_code16(desired);
        key->keycode = desired;
        key->pressed = true;
    }
}

void keyboard_post_init_user(void) {
    // Calibrate center on startup: leave the joystick untouched at boot.
    center_x = analogReadPin(JOYSTICK_X_PIN);
    center_y = analogReadPin(JOYSTICK_Y_PIN);
}

void housekeeping_task_user(void) {
    int16_t x = axis_value(analogReadPin(JOYSTICK_X_PIN), center_x);
    int16_t y = -axis_value(analogReadPin(JOYSTICK_Y_PIN), center_y);

    uint16_t left = KC_LEFT, right = KC_RIGHT;
    uint16_t up = KC_UP, down = KC_DOWN;

    switch (get_highest_layer(layer_state | default_layer_state)) {
        case 1: // Hold GP3: WASD navigation
            left = KC_A; right = KC_D; up = KC_W; down = KC_S;
            break;
        case 2: // Hold GP4: page/document navigation
            left = KC_HOME; right = KC_END;
            up = KC_PGUP; down = KC_PGDN;
            break;
        default: // Base: arrow keys
            break;
    }

    update_held_key(&left_key, left, x < -JOYSTICK_DIRECTION_THRESHOLD);
    update_held_key(&right_key, right, x > JOYSTICK_DIRECTION_THRESHOLD);
    update_held_key(&up_key, up, y < -JOYSTICK_DIRECTION_THRESHOLD);
    update_held_key(&down_key, down, y > JOYSTICK_DIRECTION_THRESHOLD);
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // GP2: Select/Enter   GP3: momentary Layer 1   GP4: momentary Layer 2
    [0] = LAYOUT(KC_ENT, MO(1), MO(2)),
    [1] = LAYOUT(KC_SPC, MO(1), MO(2)),
    [2] = LAYOUT(KC_ESC, MO(1), MO(2))
};
