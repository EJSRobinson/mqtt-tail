#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

// ------------------- LED Setup -------------------
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 300
#define DATA_PIN 2  // D4 on ESP8266 (Tentacle pin)
// #define DATA_PIN 15  // D4 on ESP8266 (Test Pin)

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
  MODE_YELLOW,
  MODE_ORANGE,
  MODE_XMAS0,
  MODE_XMAS1,
  MODE_XMAS2,
  MODE_HALLOWEEN0,
  MODE_HALLOWEEN1,
  MODE_HALLOWEEN2,
  MODE_HALLOWEEN3,
  MODE_HALLOWEEN4
};

// Halloween effect declarations
void showHalloween0();
void showHalloween1();
void showHalloween2();
void showHalloween3();
void handleHalloween0();
void handleHalloween1();
void handleHalloween2();
void handleHalloween3();
void showHalloween4();
void handleHalloween4();
DisplayMode currentMode = MODE_NONE;
int electricityPos = 0;
bool electricityDirUp = true;

// --- WiFi Setup ---





// Home Configuration

// const char* ssid = "VM8947480";
// const char* password = "phnX7zzjwpwkk2ow";
// IPAddress local_IP(192, 168, 0, 199);      // Change last octet as needed
// IPAddress gateway(192, 168, 0, 1);
// IPAddress subnet(255, 255, 255, 0); // Standard subnet for home networks

// Portable Configuration

const char* ssid = "danlers1234";
const char* password = "Danlers1234";
IPAddress subnet(255, 255, 255, 240);
IPAddress local_IP(172, 20, 10, 10);      // Change last octet as needed
IPAddress gateway(172, 20, 10, 0);


unsigned long lastFrameTime = 0;
const int frameInterval = 30; // ms

ESP8266WebServer server(80);
const char* relayHosts[] = {"172.20.10.11", "172.20.10.12", "172.20.10.13"};

bool isRelayRequest() {
  if (!server.hasArg("relay")) return false;
  String relayValue = server.arg("relay");
  return relayValue.equalsIgnoreCase("true") || relayValue == "1";
}

String buildRelayQueryString() {
  String query = "";
  for (int i = 0; i < server.args(); i++) {
    String name = server.argName(i);
    if (name.equalsIgnoreCase("relay")) continue;
    if (query.length() == 0) {
      query += "?";
    } else {
      query += "&";
    }
    query += name + "=" + server.arg(i);
  }
  return query;
}

void relayRequestToHost(const char* host) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Relay skipped: WiFi not connected");
    return;
  }

  String url = String("http://") + host + server.uri() + buildRelayQueryString();
  Serial.print("Relay request URL: ");
  Serial.println(url);

  HTTPClient http;
  WiFiClient client;
  if (!http.begin(client, url)) {
    Serial.print("Relay begin failed for host: ");
    Serial.println(host);
    return;
  }

  int httpCode = http.GET();
  Serial.print("Relay host ");
  Serial.print(host);
  Serial.print(" returned code ");
  Serial.println(httpCode);
  http.end();
}

void maybeRelay() {
  if (!isRelayRequest()) {
    Serial.println("maybeRelay: relay disabled");
    return;
  }

  Serial.println("maybeRelay: relay enabled");
  for (unsigned int i = 0; i < sizeof(relayHosts) / sizeof(relayHosts[0]); i++) {
    relayRequestToHost(relayHosts[i]);
  }
}

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
void showYellow();
void handleYellow() {
  currentMode = MODE_YELLOW;
  server.send(200, "text/plain", "Yellow effect triggered");
}
void showOrange();
void handleOrange() {
  currentMode = MODE_ORANGE;
  server.send(200, "text/plain", "Orange effect triggered");
}
// Christmas effect declarations and handlers
void showChristmas0();
void showChristmas1();
void showChristmas2();
void handleChristmas0() {
  currentMode = MODE_XMAS0;
  server.send(200, "text/plain", "Christmas 0 effect triggered");
}
void handleChristmas1() {
  currentMode = MODE_XMAS1;
  server.send(200, "text/plain", "Christmas 1 effect triggered");
}
void handleChristmas2() {
  currentMode = MODE_XMAS2;
  server.send(200, "text/plain", "Christmas 2 effect triggered");
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

void showYellow() {
  // Warm yellow fill with subtle amber flicker
  fill_solid(leds, NUM_LEDS, CRGB(200, 170, 0));
  // Add brighter yellow/amber highlights
  for (int i = 0; i < NUM_LEDS / 6; i++) {
    int idx = random(NUM_LEDS);
    uint8_t r = 220 + random8(35); // 220-255
    uint8_t g = 180 + random8(60); // 180-239
    uint8_t b = random8(24);       // small blue component
    leds[idx] = CRGB(r, g, b);
  }
  // Occasional white-hot sparkles
  for (int i = 0; i < NUM_LEDS / 40; i++) {
    int idx = random(NUM_LEDS);
    leds[idx] = CRGB(255, 255, 200);
  }
}

void showOrange() {
}
// --- Christmas Effects ---
// Christmas 0: Alternating red/green blocks with white twinkles
void showChristmas0() {
  const int blockSize = 10;
  for (int i = 0; i < NUM_LEDS; i++) {
    bool isRed = ((i / blockSize) % 2 == 0);
    leds[i] = isRed ? CRGB(255, 0, 0) : CRGB(0, 255, 0);
    // add occasional white twinkles
    if (random8(90) == 0) {
      leds[i] = CRGB::White;
    }
  }
}

// Christmas 1: Global red/green flash pulse with sparkles
void showChristmas1() {
  static unsigned long lastToggle = 0;
  static bool phaseRed = true;
  unsigned long now = millis();
  const unsigned long interval = 300; // ms
  if (now - lastToggle > interval) {
    lastToggle = now;
    phaseRed = !phaseRed;
  }
  CRGB base = phaseRed ? CRGB(255, 0, 0) : CRGB(0, 255, 0);
  fill_solid(leds, NUM_LEDS, base);
  // add bright sparkles of opposite color and some white
  for (int i = 0; i < NUM_LEDS / 12; i++) {
    int idx = random(NUM_LEDS);
    leds[idx] = phaseRed ? CRGB(0, 255, 0) : CRGB(255, 0, 0);
  }
  for (int i = 0; i < NUM_LEDS / 40; i++) {
    int idx = random(NUM_LEDS);
    leds[idx] = CRGB::White;
  }
}

// Christmas 2: Moving candy-cane stripes (red/white) with green sparkles
void showChristmas2() {
  static uint8_t offset = 0;
  const uint8_t stripeWidth = 4; // pixels per color stripe
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t pos = (i + offset) % (stripeWidth * 2);
    bool redStripe = pos < stripeWidth;
    leds[i] = redStripe ? CRGB(255, 0, 0) : CRGB(255, 255, 255);
  }
  // sprinkle some green
  for (int i = 0; i < NUM_LEDS / 18; i++) {
    int idx = random(NUM_LEDS);
    leds[idx] = CRGB(0, 255, 0);
  }
  offset++;
}
// --- Halloween Effects ---
// Halloween 0: Alternating orange and purple, with random white flashes
void showHalloween0() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if ((i / 10) % 2 == 0) {
      leds[i] = CRGB(255, 40, 10); // more red-orange
    } else {
      leds[i] = CRGB(120, 0, 180); // purple
    }
    if (random8(60) == 0) {
      leds[i] = CRGB::White;
    }
  }
}

// Halloween 1: Orange base, purple "ghost" pulses moving along the strip
void showHalloween1() {
  static uint8_t ghostPos = 0;
  fill_solid(leds, NUM_LEDS, CRGB(255, 40, 10));
  for (int i = 0; i < 5; i++) {
    int pos = (ghostPos + i * (NUM_LEDS / 5)) % NUM_LEDS;
    for (int j = -2; j <= 2; j++) {
      int idx = pos + j;
      if (idx >= 0 && idx < NUM_LEDS) {
        leds[idx] = CRGB(120, 0, 180); // purple ghost
      }
    }
  }
  ghostPos = (ghostPos + 1) % NUM_LEDS;
}

// Halloween 2: Purple base, random orange and green "pumpkin" flashes
void showHalloween2() {
  fill_solid(leds, NUM_LEDS, CRGB(120, 0, 180));
  for (int i = 0; i < NUM_LEDS / 12; i++) {
    int idx = random(NUM_LEDS);
    if (random8(2) == 0) {
      leds[idx] = CRGB(255, 40, 10); // more red-orange
    } else {
      leds[idx] = CRGB(0, 255, 40); // green
    }
  }
}

// Halloween 3: Flickering candle effect with orange/yellow, occasional purple flashes
void showHalloween3() {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t flicker = random8(40);
    leds[i] = CRGB(255, 60 + flicker, 10 + random8(20)); // more red-orange flicker
    if (random8(80) == 0) {
      leds[i] = CRGB(120, 0, 180); // purple flash
    }
  }
}
// Halloween 4: Blocks of color flash up and down randomly
void showHalloween4() {
  static uint8_t blockState[30] = {0};
  static int blockDir[30] = {0};
  static uint8_t blockColor[30] = {0};
  static unsigned long lastUpdate = 0;
  const int blockSize = 10;
  const int numBlocks = NUM_LEDS / blockSize;
  unsigned long now = millis();
  if (now - lastUpdate > 80) {
    lastUpdate = now;
    for (int b = 0; b < numBlocks; b++) {
      if (blockState[b] == 0 && random8(18) == 0) {
        blockDir[b] = random8(2) ? 1 : -1;
        blockColor[b] = random8(3); // 0=orange, 1=purple, 2=green
        blockState[b] = blockDir[b] > 0 ? 1 : blockSize;
      } else if (blockState[b] > 0) {
        blockState[b] += blockDir[b];
        if (blockState[b] > blockSize || blockState[b] < 1) blockState[b] = 0;
      }
    }
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  for (int b = 0; b < numBlocks; b++) {
    if (blockState[b] > 0) {
      CRGB color = CRGB::Black;
      if (blockColor[b] == 0) color = CRGB(255, 40, 10); // more red-orange
      else if (blockColor[b] == 1) color = CRGB(120, 0, 180); // purple
      else color = CRGB(0, 255, 40); // green
      int start = b * blockSize;
      int end = start + blockState[b] * blockDir[b];
      if (blockDir[b] > 0) {
        for (int i = start; i < start + blockState[b]; i++) {
          if (i < NUM_LEDS) leds[i] = color;
        }
      } else {
        for (int i = start + blockSize - 1; i >= start + blockSize - blockState[b]; i--) {
          if (i < NUM_LEDS && i >= 0) leds[i] = color;
        }
      }
    }
  }
}

void handleHalloween0() {
  currentMode = MODE_HALLOWEEN0;
  server.send(200, "text/plain", "Halloween 0 effect triggered");
}
void handleHalloween1() {
  currentMode = MODE_HALLOWEEN1;
  server.send(200, "text/plain", "Halloween 1 effect triggered");
}
void handleHalloween2() {
  currentMode = MODE_HALLOWEEN2;
  server.send(200, "text/plain", "Halloween 2 effect triggered");
}
void handleHalloween3() {
  currentMode = MODE_HALLOWEEN3;
  server.send(200, "text/plain", "Halloween 3 effect triggered");
}
void handleHalloween4() {
  currentMode = MODE_HALLOWEEN4;
  server.send(200, "text/plain", "Halloween 4 effect triggered");
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
  // Ensure bolts array exists so the effect can run immediately after triggering
  if (!bolts) {
    bolts = new Bolt[NUM_BOLTS];
    boltsInitialized = false;
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
  // Ensure bolts array exists so the effect can run immediately after triggering
  if (!bolts) {
    bolts = new Bolt[NUM_BOLTS];
    boltsInitialized = false;
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

void handleRelayCallback(void (*handler)()) {
  Serial.print("Incoming request: ");
  Serial.print(server.uri());
  Serial.print(" args=");
  Serial.print(server.args());
  Serial.print(" relay=");
  Serial.println(server.hasArg("relay") ? server.arg("relay") : "false");
  maybeRelay();
  handler();
}

void setup() {
  server.on("/halloween4", HTTP_GET, []() { handleRelayCallback(handleHalloween4); });
  server.on("/halloween0", HTTP_GET, []() { handleRelayCallback(handleHalloween0); });
  server.on("/halloween1", HTTP_GET, []() { handleRelayCallback(handleHalloween1); });
  server.on("/halloween2", HTTP_GET, []() { handleRelayCallback(handleHalloween2); });
  server.on("/halloween3", HTTP_GET, []() { handleRelayCallback(handleHalloween3); });
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting setup...");
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(70);
  showBootStatus(CRGB::Red); // Show red status lights
  FastLED.show();
  setup_wifi();
  server.on("/rainbow", HTTP_GET, []() { handleRelayCallback(handleRainbow); });
  server.on("/electricity", HTTP_GET, []() { handleRelayCallback(handleElectricity); });
  server.on("/electricity2", HTTP_GET, []() { handleRelayCallback(handleElectricity2); });
  server.on("/off", HTTP_GET, []() { handleRelayCallback(handleOff); });
  server.on("/brightness", HTTP_GET, []() { handleRelayCallback(handleBrightness); });
  server.on("/fire", HTTP_GET, []() { handleRelayCallback(handleFire); });
  server.on("/firerainbow", HTTP_GET, []() { handleRelayCallback(handleFireRainbow); });
  server.on("/red", HTTP_GET, []() { handleRelayCallback(handleRed); });
  server.on("/green", HTTP_GET, []() { handleRelayCallback(handleGreen); });
  server.on("/blue", HTTP_GET, []() { handleRelayCallback(handleBlue); });
  server.on("/purple", HTTP_GET, []() { handleRelayCallback(handlePurple); });
  server.on("/pink", HTTP_GET, []() { handleRelayCallback(handlePink); });
  server.on("/yellow", HTTP_GET, []() { handleRelayCallback(handleYellow); });
  server.on("/orange", HTTP_GET, []() { handleRelayCallback(handleOrange); });
  server.on("/christmas0", HTTP_GET, []() { handleRelayCallback(handleChristmas0); });
  server.on("/christmas1", HTTP_GET, []() { handleRelayCallback(handleChristmas1); });
  server.on("/christmas2", HTTP_GET, []() { handleRelayCallback(handleChristmas2); });
  // Add more endpoints here as needed
  server.begin();
  Serial.println("HTTP server started");
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
      case MODE_YELLOW:
        showYellow();
        FastLED.show();
        break;
      case MODE_ORANGE:
        showOrange();
        FastLED.show();
        break;
      case MODE_XMAS0:
        showChristmas0();
        FastLED.show();
        break;
      case MODE_XMAS1:
        showChristmas1();
        FastLED.show();
        break;
      case MODE_XMAS2:
        showChristmas2();
        FastLED.show();
        break;
      case MODE_HALLOWEEN0:
        showHalloween0();
        FastLED.show();
        break;
      case MODE_HALLOWEEN1:
        showHalloween1();
        FastLED.show();
        break;
      case MODE_HALLOWEEN2:
        showHalloween2();
        FastLED.show();
        break;
      case MODE_HALLOWEEN3:
        showHalloween3();
        FastLED.show();
        break;
      case MODE_HALLOWEEN4:
        showHalloween4();
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
