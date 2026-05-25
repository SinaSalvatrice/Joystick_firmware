#include QMK_KEYBOARD_H
#include "analog.h"
#include "gpio.h"
#ifdef RGBLIGHT_ENABLE
#    include "rgblight.h"
#    include "timer.h"
#endif
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

#ifdef RGBLIGHT_ENABLE
static uint32_t led_animation_timer = 0;
static uint8_t  led_head_index      = 0;
static bool     leds_active         = false;

static uint16_t axis_magnitude(int16_t axis_x, int16_t axis_y) {
    uint16_t abs_x = (axis_x < 0) ? (uint16_t)(-axis_x) : (uint16_t)axis_x;
    uint16_t abs_y = (axis_y < 0) ? (uint16_t)(-axis_y) : (uint16_t)axis_y;
    return abs_x + abs_y;
}

static uint16_t led_update_interval_ms(uint16_t magnitude) {
    if (magnitude >= 254) {
        return 20;
    }

    // Range: 0..254 -> 150..20 ms (faster when stick is further from center).
    return (uint16_t)(150 - (magnitude * 130U) / 254U);
}

static int8_t led_rotation_step(int16_t axis_x, int16_t axis_y) {
    int16_t dominant = axis_x;
    uint16_t abs_x   = (axis_x < 0) ? (uint16_t)(-axis_x) : (uint16_t)axis_x;
    uint16_t abs_y   = (axis_y < 0) ? (uint16_t)(-axis_y) : (uint16_t)axis_y;

    if (abs_y > abs_x) {
        dominant = axis_y;
    }

    if (dominant < 0) {
        return -1;
    }

    return 1;
}

static void render_wandering_light(uint8_t head_index) {
    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        rgblight_setrgb_at(0, 0, 0, i);
    }

    const uint8_t left  = (uint8_t)((head_index + RGBLIGHT_LED_COUNT - 1) % RGBLIGHT_LED_COUNT);
    const uint8_t right = (uint8_t)((head_index + 1) % RGBLIGHT_LED_COUNT);

    rgblight_setrgb_at(255, 0, 255, head_index);
    rgblight_setrgb_at(64, 0, 64, left);
    rgblight_setrgb_at(64, 0, 64, right);

    rgblight_set();
    leds_active = true;
}

static void render_leds_off(void) {
    if (!leds_active) {
        return;
    }

    for (uint8_t i = 0; i < RGBLIGHT_LED_COUNT; i++) {
        rgblight_setrgb_at(0, 0, 0, i);
    }

    rgblight_set();
    leds_active = false;
}
#endif

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

#ifdef RGBLIGHT_ENABLE
    rgblight_enable_noeeprom();
    rgblight_mode_noeeprom(RGBLIGHT_MODE_STATIC_LIGHT);
    led_animation_timer = timer_read32();
    leds_active         = true;
    render_leds_off();
#endif
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

#ifdef RGBLIGHT_ENABLE
    const uint16_t magnitude = axis_magnitude(axis_x, axis_y);
    if (magnitude == 0) {
        render_leds_off();
        return;
    }

    const uint16_t interval_ms = led_update_interval_ms(magnitude);
    if (timer_elapsed32(led_animation_timer) < interval_ms) {
        return;
    }

    led_animation_timer = timer_read32();
    led_head_index      = (uint8_t)((led_head_index + led_rotation_step(axis_x, axis_y) + RGBLIGHT_LED_COUNT) % RGBLIGHT_LED_COUNT);
    render_wandering_light(led_head_index);
#endif
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
