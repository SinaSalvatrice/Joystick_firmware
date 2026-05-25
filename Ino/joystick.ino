#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <led_strip.h>


// =====================================================
// OLED
// =====================================================
#define OLED_SDA 5
#define OLED_SCL 6

// Falls OLED  bleibt:
// #define OLED_SDA 8
// #define OLED_SCL 9

U8G2_SSD1306_72X40_ER_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// Für 128x64 OLED stattdessen:
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);


// =====================================================
// Joystick + Buttons
// =====================================================
#define JOY_X   0
#define JOY_Y   1
#define JOY_SW  4

#define BTN_1   3
#define BTN_2   10


// =====================================================
// WS2812 LEDs
// =====================================================
#define LED_PIN     7
#define LED_COUNT   18
#define LED_BRIGHT  35   // 0-255, erstmal niedrig lassen

led_strip_handle_t strip;


// =====================================================
// LED Hilfsfunktionen
// =====================================================
uint8_t dim(uint8_t value) {
  return (uint16_t)value * LED_BRIGHT / 255;
}

void setLed(int index, uint8_t r, uint8_t g, uint8_t b) {
  if (index < 0 || index >= LED_COUNT) return;
  led_strip_set_pixel(strip, index, dim(r), dim(g), dim(b));
}

void clearLedsBuffer() {
  for (int i = 0; i < LED_COUNT; i++) {
    setLed(i, 0, 0, 0);
  }
}

void fillLeds(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < LED_COUNT; i++) {
    setLed(i, r, g, b);
  }
}

void showLeds() {
  led_strip_refresh(strip);
}

void setupLedStrip() {
  led_strip_config_t strip_config = {};
  strip_config.strip_gpio_num = LED_PIN;
  strip_config.max_leds = LED_COUNT;
  strip_config.led_model = LED_MODEL_WS2812;
  strip_config.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
  strip_config.flags.invert_out = false;

  led_strip_rmt_config_t rmt_config = {};
  rmt_config.clk_src = RMT_CLK_SRC_DEFAULT;
  rmt_config.resolution_hz = 10 * 1000 * 1000;
  rmt_config.mem_block_symbols = 64;
  rmt_config.flags.with_dma = false;

  led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);

  led_strip_clear(strip);
}

void startupAnimation() {
  clearLedsBuffer();

  for (int i = 0; i < LED_COUNT; i++) {
    setLed(i, 0, 0, 255);
    showLeds();
    delay(25);
  }

  delay(150);
  led_strip_clear(strip);
}


// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(JOY_SW, INPUT_PULLUP);
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);

  Wire.begin(OLED_SDA, OLED_SCL);

  oled.begin();
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);
  oled.drawStr(0, 10, "ESP32-C3");
  oled.drawStr(0, 22, "OLED + Joy");
  oled.drawStr(0, 34, "LED Strip");
  oled.sendBuffer();

  setupLedStrip();
  startupAnimation();

  delay(500);
}


// =====================================================
// Loop
// =====================================================
void loop() {
  int xRaw = analogRead(JOY_X);
  int yRaw = analogRead(JOY_Y);

  int xPercent = map(xRaw, 0, 4095, 0, 100);
  int yPercent = map(yRaw, 0, 4095, 0, 100);

  xPercent = constrain(xPercent, 0, 100);
  yPercent = constrain(yPercent, 0, 100);

  bool joyPressed  = digitalRead(JOY_SW) == LOW;
  bool btn1Pressed = digitalRead(BTN_1) == LOW;
  bool btn2Pressed = digitalRead(BTN_2) == LOW;

  int ledIndex = map(xRaw, 0, 4095, 0, LED_COUNT - 1);
  ledIndex = constrain(ledIndex, 0, LED_COUNT - 1);

  const char* direction = "MID";

  if (xPercent < 35) {
    direction = "LEFT";
  } else if (xPercent > 65) {
    direction = "RIGHT";
  } else if (yPercent < 35) {
    direction = "UP";
  } else if (yPercent > 65) {
    direction = "DOWN";
  }

  // =====================================================
  // LED Logik
  // =====================================================
  clearLedsBuffer();

  if (btn1Pressed) {
    // Button 1: rot
    fillLeds(255, 0, 0);
  } 
  else if (btn2Pressed) {
    // Button 2: blau
    fillLeds(0, 0, 255);
  } 
  else if (joyPressed) {
    // Joystick gedrueckt: gruen
    fillLeds(0, 255, 0);
  } 
  else {
    // Normal: LED-Punkt wandert mit X-Achse
    setLed(ledIndex, 160, 0, 255);

    if (ledIndex > 0) {
      setLed(ledIndex - 1, 40, 0, 80);
    }

    if (ledIndex < LED_COUNT - 1) {
      setLed(ledIndex + 1, 40, 0, 80);
    }
  }

  showLeds();

  // =====================================================
  // OLED Ausgabe
  // =====================================================
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x8_tf);

  oled.setCursor(0, 8);
  oled.print("X:");
  oled.print(xPercent);
  oled.print("%");

  oled.setCursor(38, 8);
  oled.print("Y:");
  oled.print(yPercent);
  oled.print("%");

  oled.setCursor(0, 18);
  oled.print("J:");
  oled.print(joyPressed ? "ON " : "OFF");

  oled.setCursor(38, 18);
  oled.print("B1:");
  oled.print(btn1Pressed ? "ON" : "OFF");

  oled.setCursor(0, 28);
  oled.print("B2:");
  oled.print(btn2Pressed ? "ON " : "OFF");

  oled.setCursor(38, 28);
  oled.print("L:");
  oled.print(ledIndex);

  oled.setCursor(0, 38);
  oled.print(direction);

  oled.sendBuffer();

  // =====================================================
  // Serial Debug
  // =====================================================
  Serial.print("X:");
  Serial.print(xRaw);
  Serial.print(" Y:");
  Serial.print(yRaw);
  Serial.print(" LED:");
  Serial.print(ledIndex);
  Serial.print(" DIR:");
  Serial.print(direction);
  Serial.print(" Joy:");
  Serial.print(joyPressed);
  Serial.print(" B1:");
  Serial.print(btn1Pressed);
  Serial.print(" B2:");
  Serial.println(btn2Pressed);

  delay(50);
}