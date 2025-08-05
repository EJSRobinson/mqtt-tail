#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ------------------- LED Setup -------------------
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 300
#define DATA_PIN 2  // D4 on ESP8266

CRGB leds[NUM_LEDS];

// --- Fire color palette ---
const CRGB firePalette[] = {
  CRGB(0x10, 0x00, 0x00), // deep red
  CRGB(0x80, 0x10, 0x00), // red
  CRGB(0xFF, 0x40, 0x00), // orange
  CRGB(0xFF, 0xA0, 0x00), // yellow-orange
  CRGB(0xFF, 0xFF, 0x20), // yellow
  CRGB(0xFF, 0xFF, 0xFF)  // white
};
const int firePaletteSize = sizeof(firePalette) / sizeof(CRGB);

// --- Electricity effect globals ---
uint8_t NUM_BOLTS = 10;
struct Bolt {
  int pos;
  int len;
  int speed;
  bool dirUp;
};
static Bolt* bolts = nullptr;
static bool boltsInitialized = false;
uint8_t plasmaBaseSpeed = 3; // baseline speed, can be changed via API or code

// --- Effect state ---
enum DisplayMode {
  MODE_NONE,
  MODE_RAINBOW,
  MODE_ELECTRICITY,
  MODE_ELECTRICITY2,
  MODE_FIRE,
  MODE_FIRERAINBOW,
  MODE_RED,
  MODE_GREEN,
  MODE_BLUE,
  MODE_PURPLE,
  MODE_PINK,
  MODE_ORANGE
};
DisplayMode currentMode = MODE_NONE;
int electricityPos = 0;
bool electricityDirUp = true;

// --- WiFi Setup ---

const char* ssid = "danlers1234";
const char* password = "Danlers1234";

// Static IP configuration
IPAddress local_IP(172, 20, 10, 10);      // Change last octet as needed
IPAddress gateway(172, 20, 10, 0);
// IPAddress local_IP(192, 168, 0, 199);      // Change last octet as needed
// IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 240);
// IPAddress subnet(255, 255, 255, 0); // Standard subnet for home networks


unsigned long lastFrameTime = 0;
const int frameInterval = 30; // ms

ESP8266WebServer server(80);

// ------------------- Function Declarations -------------------
void setup_wifi();
void showRainbow();
void showElectricity();
void showBootStatus(CRGB color);
void handleRainbow();
void handleElectricity();
void handleOff();
void handleBrightness();
void showFire();
void showFireRainbow();
void handleFire() {
  currentMode = MODE_FIRE;
  server.send(200, "text/plain", "Fire effect triggered");
}
void handleFireRainbow() {
  currentMode = MODE_FIRERAINBOW;
  server.send(200, "text/plain", "FireRainbow effect triggered");
}
void showRed();
void handleRed() {
  currentMode = MODE_RED;
  server.send(200, "text/plain", "Red effect triggered");
}
void showGreen();
void handleGreen() {
  currentMode = MODE_GREEN;
  server.send(200, "text/plain", "Green effect triggered");
}
void showBlue();
void handleBlue() {
  currentMode = MODE_BLUE;
  server.send(200, "text/plain", "Blue effect triggered");
}
void showPurple();
void handlePurple() {
  currentMode = MODE_PURPLE;
  server.send(200, "text/plain", "Purple effect triggered");
}
void showPink();
void handlePink() {
  currentMode = MODE_PINK;
  server.send(200, "text/plain", "Pink effect triggered");
}
void showOrange();
void handleOrange() {
  currentMode = MODE_ORANGE;
  server.send(200, "text/plain", "Orange effect triggered");
}

// ------------------- Boot Status Lights -------------------
void showBootStatus(CRGB color) {
  FastLED.clear();
  for (int i = 0; i < 10; i++) {
    int idx = i * 30;
    if (idx < NUM_LEDS) {
      leds[idx] = color;
    }
  }
  FastLED.show();
}

// ------------------- Rainbow Effect -------------------
void showRainbow() {
  static uint8_t startHue = 0;
  // Only compute for first half, then mirror
  for (int i = 0; i < 150; i++) {
    uint8_t hue = startHue + (i * 255 / 149); // 0..149
    leds[i] = CHSV(hue, 255, 255);
    leds[NUM_LEDS - 1 - i] = CHSV(hue, 255, 255); // Mirror to 299..150
  }
  startHue++;
}

// ------------------- Electricity Effect -------------------
// --- Improved Plasma Electricity Effect ---
void showElectricity() {
  if (!bolts) return;
  if (!boltsInitialized) {
    for (int i = 0; i < NUM_BOLTS; i++) {
      bolts[i].len = 8 + random8(8); // 8-15
      bolts[i].pos = random(NUM_LEDS - bolts[i].len);
      bolts[i].speed = plasmaBaseSpeed + random8(3); // baseline + random
      bolts[i].dirUp = random8(2);
    }
    boltsInitialized = true;
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int b = 0; b < NUM_BOLTS; b++) {
    for (int i = 0; i < bolts[b].len; i++) {
      int idx = bolts[b].dirUp ? bolts[b].pos + i : bolts[b].pos - i;
      if (idx >= 0 && idx < NUM_LEDS) {
        // Plasma color: flicker between blue/white
        uint8_t blue = 180 + random8(75);
        uint8_t white = 200 + random8(55);
        if (random8(3) == 0) {
          leds[idx] = CRGB(white, white, blue); // white-blue
        } else {
          leds[idx] = CRGB(50, 100, blue); // blue tone
        }
      }
    }
    // Move bolt
    if (bolts[b].dirUp) {
      bolts[b].pos += bolts[b].speed;
      if (bolts[b].pos > NUM_LEDS - bolts[b].len) {
        bolts[b].dirUp = false;
        bolts[b].pos = NUM_LEDS - bolts[b].len;
        bolts[b].len = 8 + random8(8);
        bolts[b].speed = plasmaBaseSpeed + random8(3);
      }
    } else {
      bolts[b].pos -= bolts[b].speed;
      if (bolts[b].pos < 0) {
        bolts[b].dirUp = true;
        bolts[b].pos = 0;
        bolts[b].len = 8 + random8(8);
        bolts[b].speed = plasmaBaseSpeed + random8(3);
      }
    }
  }
}

// --- Smoother, more colorful electricity effect ---
void showElectricity2() {
  if (!bolts) return;
  if (!boltsInitialized) {
    for (int i = 0; i < NUM_BOLTS; i++) {
      bolts[i].len = 10 + random8(10); // 10-19, longer bolts
      bolts[i].pos = random(NUM_LEDS - bolts[i].len);
      bolts[i].speed = plasmaBaseSpeed + random8(2); // slightly slower
      bolts[i].dirUp = random8(2);
    }
    boltsInitialized = true;
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int b = 0; b < NUM_BOLTS; b++) {
    // Pick a base color for the bolt
    uint8_t baseHue = random8();
    for (int i = 0; i < bolts[b].len; i++) {
      int idx = bolts[b].dirUp ? bolts[b].pos + i : bolts[b].pos - i;
      if (idx >= 0 && idx < NUM_LEDS) {
        // Smoother color gradient along the bolt
        uint8_t hue = baseHue + i * 8 + random8(8); // gradient with randomness
        uint8_t sat = 200 + random8(55); // vibrant
        uint8_t val = 180 + random8(75); // bright
        leds[idx] = CHSV(hue, sat, val);
        // Occasional white-hot flicker
        if (random8(12) == 0) {
          leds[idx] = CRGB(255, 255, 255);
        }
      }
    }
    // Move bolt smoothly (fractional speed)
    if (bolts[b].dirUp) {
      bolts[b].pos += bolts[b].speed;
      if (bolts[b].pos > NUM_LEDS - bolts[b].len) {
        bolts[b].dirUp = false;
        bolts[b].pos = NUM_LEDS - bolts[b].len;
        bolts[b].len = 10 + random8(10);
        bolts[b].speed = plasmaBaseSpeed + random8(2);
      }
    } else {
      bolts[b].pos -= bolts[b].speed;
      if (bolts[b].pos < 0) {
        bolts[b].dirUp = true;
        bolts[b].pos = 0;
        bolts[b].len = 10 + random8(10);
        bolts[b].speed = plasmaBaseSpeed + random8(2);
      }
    }
  }
}

void showFire() {
  for (int i = 0; i < NUM_LEDS; i++) {
    // Flicker: random palette index, plus some bias for lower LEDs
    int baseIdx = map(i, 0, NUM_LEDS - 1, 0, firePaletteSize - 2);
    int flicker = random8(3) - 1; // -1, 0, 1
    int colorIdx = constrain(baseIdx + flicker, 0, firePaletteSize - 1);
    // Extra white flicker at top
    if (i > NUM_LEDS * 3 / 4 && random8(8) == 0) colorIdx = firePaletteSize - 1;
    leds[i] = firePalette[colorIdx];
  }
  // Randomly dim some pixels for more flicker
  for (int i = 0; i < NUM_LEDS / 10; i++) {
    int idx = random(NUM_LEDS);
    leds[idx].fadeToBlackBy(random8(128));
  }
}

void showFireRainbow() {
  // 🌀 Animate by offsetting the position along the strip
  static uint8_t positionOffset = 0;

  // 🎨 Define the hue range for the effect
  const uint8_t hueStart = 0;    // Red
  const uint8_t hueEnd = 64;     // Yellow
  const uint8_t hueRange = hueEnd - hueStart;

  // 💡 Total LEDs to fill before mirroring
  const int halfCount = NUM_LEDS / 2;

  for (int i = 0; i < halfCount; i++) {
    // 🧠 Calculate a position that wraps around [0, halfCount)
    int shiftedIndex = (i + positionOffset) % halfCount;

    // 🎯 Map this position into the desired hue range
    uint8_t hue = hueStart + ((shiftedIndex * hueRange) / (halfCount - 1));

    // 🖍️ Set symmetric/mirrored colors
    leds[i] = CHSV(hue, 255, 255);
    leds[NUM_LEDS - 1 - i] = CHSV(hue, 255, 255);
  }

  // 🔁 Increment position for smooth scrolling effect
  positionOffset++;
}

void showRed() {
  // Fill with deep red
  fill_solid(leds, NUM_LEDS, CRGB(180, 0, 0));
  // Add random reddish flickers
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 180 + random8(75); // 180-255
    uint8_t g = random8(30);       // 0-29
    uint8_t b = random8(20);       // 0-19
    leds[idx] = CRGB(r, g, b);
  }
}

void showGreen() {
  fill_solid(leds, NUM_LEDS, CRGB(0, 180, 0));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t g = 180 + random8(75);
    uint8_t r = random8(30);
    uint8_t b = random8(20);
    leds[idx] = CRGB(r, g, b);
  }
}

void showBlue() {
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 180));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t b = 180 + random8(75);
    uint8_t r = random8(20);
    uint8_t g = random8(30);
    leds[idx] = CRGB(r, g, b);
  }
}

void showPurple() {
  fill_solid(leds, NUM_LEDS, CRGB(120, 0, 180));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 120 + random8(60);
    uint8_t b = 180 + random8(75);
    uint8_t g = random8(20);
    leds[idx] = CRGB(r, g, b);
  }
}

void showPink() {
  fill_solid(leds, NUM_LEDS, CRGB(255, 20, 120));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 200 + random8(55);
    uint8_t g = 20 + random8(30);
    uint8_t b = 120 + random8(60);
    leds[idx] = CRGB(r, g, b);
  }
}

void showOrange() {
  fill_solid(leds, NUM_LEDS, CRGB(255, 120, 0));
  for (int i = 0; i < NUM_LEDS / 8; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 200 + random8(55);
    uint8_t g = 120 + random8(60);
    uint8_t b = random8(20);
    leds[idx] = CRGB(r, g, b);
  }
}

void handleRainbow() {
  currentMode = MODE_RAINBOW;
  server.send(200, "text/plain", "Rainbow updated");
}

void handleElectricity() {
  // Parse query params
  String speedParam = server.hasArg("speed") ? server.arg("speed") : "";
  String boltsParam = server.hasArg("bolts") ? server.arg("bolts") : "";
  if (speedParam.length() > 0) {
    plasmaBaseSpeed = constrain(speedParam.toInt(), 1, 10);
  }
  if (boltsParam.length() > 0) {
    uint8_t newBolts = constrain(boltsParam.toInt(), 1, 30);
    if (newBolts != NUM_BOLTS) {
      NUM_BOLTS = newBolts;
      if (bolts) delete[] bolts;
      bolts = new Bolt[NUM_BOLTS];
      boltsInitialized = false;
    }
  }
  currentMode = MODE_ELECTRICITY;
  server.send(200, "text/plain", "Electricity effect triggered");
}

void handleElectricity2() {
  // Parse query params
  String speedParam = server.hasArg("speed") ? server.arg("speed") : "";
  String boltsParam = server.hasArg("bolts") ? server.arg("bolts") : "";
  if (speedParam.length() > 0) {
    plasmaBaseSpeed = constrain(speedParam.toInt(), 1, 10);
  }
  if (boltsParam.length() > 0) {
    uint8_t newBolts = constrain(boltsParam.toInt(), 1, 30);
    if (newBolts != NUM_BOLTS) {
      NUM_BOLTS = newBolts;
      if (bolts) delete[] bolts;
      bolts = new Bolt[NUM_BOLTS];
      boltsInitialized = false;
    }
  }
  currentMode = MODE_ELECTRICITY2;
  server.send(200, "text/plain", "Electricity2 effect triggered");
}

void handleOff() {
  currentMode = MODE_NONE;
  FastLED.clear();
  FastLED.show();
  server.send(200, "text/plain", "LEDs turned off");
}

void handleBrightness() {
  String val = server.hasArg("value") ? server.arg("value") : "";
  if (val.length() > 0) {
    int brightness = constrain(val.toInt(), 1, 255);
    FastLED.setBrightness(brightness);
    FastLED.show();
    server.send(200, "text/plain", String("Brightness set to ") + brightness);
  } else {
    server.send(400, "text/plain", "Missing value param");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(70);
  showBootStatus(CRGB::Red); // Show red status lights
  FastLED.show();
  setup_wifi();
  server.on("/rainbow", HTTP_GET, handleRainbow);
  server.on("/electricity", HTTP_GET, handleElectricity);
  server.on("/electricity2", HTTP_GET, handleElectricity2);
  server.on("/off", HTTP_GET, handleOff);
  server.on("/brightness", HTTP_GET, handleBrightness);
  server.on("/fire", HTTP_GET, handleFire);
  server.on("/firerainbow", HTTP_GET, handleFireRainbow);
  server.on("/red", HTTP_GET, handleRed);
  server.on("/green", HTTP_GET, handleGreen);
  server.on("/blue", HTTP_GET, handleBlue);
  server.on("/purple", HTTP_GET, handlePurple);
  server.on("/pink", HTTP_GET, handlePink);
  server.on("/orange", HTTP_GET, handleOrange);
  // Add more endpoints here as needed
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (now - lastFrameTime >= frameInterval) {
    lastFrameTime = now;
    switch (currentMode) {
      case MODE_RAINBOW:
        showRainbow();
        FastLED.show();
        break;
      case MODE_ELECTRICITY:
        showElectricity();
        FastLED.show();
        break;
      case MODE_ELECTRICITY2:
        showElectricity2();
        FastLED.show();
        break;
      case MODE_FIRE:
        showFire();
        FastLED.show();
        break;
      case MODE_FIRERAINBOW:
        showFireRainbow();
        FastLED.show();
        break;
      case MODE_RED:
        showRed();
        FastLED.show();
        break;
      case MODE_GREEN:
        showGreen();
        FastLED.show();
        break;
      case MODE_BLUE:
        showBlue();
        FastLED.show();
        break;
      case MODE_PURPLE:
        showPurple();
        FastLED.show();
        break;
      case MODE_PINK:
        showPink();
        FastLED.show();
        break;
      case MODE_ORANGE:
        showOrange();
        FastLED.show();
        break;
      case MODE_NONE:
      default:
        // Only show status lights
        break;
    }
  }
}

// ------------------- WiFi Connection -------------------
void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  // Change status lights to green
  showBootStatus(CRGB::Green);
  FastLED.show();
}
