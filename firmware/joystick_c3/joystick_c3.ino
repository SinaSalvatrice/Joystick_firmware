#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

constexpr uint8_t X_PIN = 0;
constexpr uint8_t Y_PIN = 1;
constexpr uint8_t BUTTON_PINS[] = {2, 3, 4};
constexpr uint8_t PIXEL_PIN = 5;
constexpr uint8_t PIXEL_COUNT = 3;

Adafruit_NeoPixel pixels(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

struct Button {
  bool sample;
  bool pressed;
  uint32_t changedAt;
};

Button buttons[3]{};
uint8_t pressedMask = 0;
uint8_t pressEdges = 0;
uint16_t rawX = 0, rawY = 0;
uint8_t ledMode = 0, red = 210, green = 135, blue = 92, brightness = 24;
int testPixel = -1;
uint32_t testStarted = 0, lastSample = 0;
uint32_t lastColors[PIXEL_COUNT]{};

void readInputs(uint32_t now) {
  if (now - lastSample < 5) return;
  lastSample = now;
  rawX = analogRead(X_PIN);
  rawY = analogRead(Y_PIN);
  for (uint8_t i = 0; i < 3; ++i) {
    Button &button = buttons[i];
    const bool next = digitalRead(BUTTON_PINS[i]) == LOW;
    if (next != button.sample) {
      button.sample = next;
      button.changedAt = now;
    }
    if (next == button.pressed || now - button.changedAt < 18) continue;
    button.pressed = next;
    if (next) {
      pressedMask |= 1u << i;
      pressEdges |= 1u << i;
    } else {
      pressedMask &= ~(1u << i);
    }
  }
}

uint32_t scaledColor(uint8_t r, uint8_t g, uint8_t b) {
  return pixels.Color(r * brightness / 255, g * brightness / 255, b * brightness / 255);
}

void renderPixels(uint32_t now) {
  if (testPixel >= 0 && now - testStarted >= 1200) testPixel = -1;
  const uint8_t axisPixel = static_cast<uint32_t>(rawX) * PIXEL_COUNT / 4096;
  bool changed = false;
  for (uint8_t i = 0; i < PIXEL_COUNT; ++i) {
    uint32_t color = 0;
    if (testPixel >= 0) {
      if (i == testPixel) color = pixels.Color(25, 14, 5);
    } else if (ledMode == 1 || (ledMode == 2 && i == axisPixel) ||
               (ledMode == 3 && (pressedMask & (1u << i)))) {
      color = scaledColor(red, green, blue);
    }
    if (color != lastColors[i]) changed = true;
    lastColors[i] = color;
    pixels.setPixelColor(i, color);
  }
  if (changed) pixels.show();
}

void handleCommand(const char *line) {
  if (strcmp(line, "?") == 0) {
    Serial.printf("JOY1,%u,%u,%u,%u\n", unsigned(rawX), unsigned(rawY),
                  unsigned(pressedMask), unsigned(pressEdges));
    pressEdges = 0;
    return;
  }
  int mode, r, g, b, level, index;
  char extra;
  if (sscanf(line, "L %d %d %d %d %d %c", &mode, &r, &g, &b, &level, &extra) == 5 &&
      mode >= 0 && mode <= 3 && r >= 0 && r <= 255 && g >= 0 && g <= 255 &&
      b >= 0 && b <= 255 && level >= 0 && level <= 64) {
    ledMode = mode;
    red = r;
    green = g;
    blue = b;
    brightness = level;
    Serial.println("OK L");
  } else if (sscanf(line, "T %d %c", &index, &extra) == 1 && index >= 0 && index < PIXEL_COUNT) {
    testPixel = index;
    testStarted = millis();
    Serial.printf("OK T %d\n", index);
  }
}

void readCommands() {
  static char line[48];
  static size_t length = 0;
  static bool overflow = false;
  for (uint8_t bytes = 0; bytes < 64 && Serial.available(); ++bytes) {
    const char c = Serial.read();
    if (c == '\n') {
      if (!overflow) {
        line[length] = '\0';
        handleCommand(line);
      }
      length = 0;
      overflow = false;
    } else if (c != '\r') {
      if (length < sizeof(line) - 1) line[length++] = c;
      else overflow = true;
    }
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  for (uint8_t pin : BUTTON_PINS) pinMode(pin, INPUT_PULLUP);
  pixels.begin();
  pixels.clear();
  pixels.show();
}

void loop() {
  const uint32_t now = millis();
  readInputs(now);
  readCommands();
  renderPixels(now);
}
