#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

// LED, WiFi and IP settings come from build_src_flags populated by
// env_secrets.ini (see env_secrets.example.ini).
#if !defined(CFG_NUM_LEDS) || !defined(CFG_DATA_PIN)
#error "NUM_LEDS and DATA_PIN must be set in env_secrets.ini ([leds] section)"
#endif
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD)
#error "WIFI_SSID and WIFI_PASSWORD must be set in env_secrets.ini ([secrets] section)"
#endif
#if !defined(USE_DHCP) || !defined(STATIC_IP) || !defined(GATEWAY_IP) || !defined(SUBNET_MASK)
#error "USE_DHCP, STATIC_IP, GATEWAY_IP and SUBNET_MASK must be set in env_secrets.ini ([network] section)"
#endif

// ------------------- LED Setup -------------------
#define LED_TYPE WS2812B
#define COLOR_ORDER RGB
// Build flags use a CFG_ prefix because FastLED uses DATA_PIN internally
constexpr int NUM_LEDS = CFG_NUM_LEDS;
constexpr uint8_t DATA_PIN = CFG_DATA_PIN;

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
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
bool useDHCP = USE_DHCP;
IPAddress local_IP;
IPAddress gateway;
IPAddress subnet;

unsigned long lastFrameTime = 0;
const int frameInterval = 30; // ms

ESP8266WebServer server(80);
const char* relayHosts[] = {"172.20.10.11", "172.20.10.12", "172.20.10.13"};

bool repeatEnabled = false;
unsigned long repeatIntervalMs = 15000;
unsigned long lastRepeatTime = 0;
String lastRelayPath = "";
String lastRelayQuery = "";

bool isRelayRequest() {
  if (!server.hasArg("relay")) return false;
  String relayValue = server.arg("relay");
  return relayValue.equalsIgnoreCase("true") || relayValue == "1";
}

String buildRelayQueryString() {
  String query = "";
  for (int i = 0; i < server.args(); i++) {
    String name = server.argName(i);
    if (name.equalsIgnoreCase("relay") || name.equalsIgnoreCase("repeat")) continue;
    if (query.length() == 0) {
      query += "?";
    } else {
      query += "&";
    }
    query += name + "=" + server.arg(i);
  }
  return query;
}

void relayRequestToHost(const char* host, const String& path, const String& query) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Relay skipped: WiFi not connected");
    return;
  }

  String url = String("http://") + host + path + query;
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
  if (server.uri() == "/repeat" || server.uri() == "/stopRelaying") {
    Serial.print("maybeRelay: skipping relay for control endpoint ");
    Serial.println(server.uri());
    return;
  }

  if (!isRelayRequest()) {
    Serial.println("maybeRelay: relay disabled");
    return;
  }

  lastRelayPath = server.uri();
  lastRelayQuery = buildRelayQueryString();
  Serial.print("Stored last relay command: ");
  Serial.print(lastRelayPath);
  Serial.print(" ");
  Serial.println(lastRelayQuery);

  Serial.println("maybeRelay: relay enabled");
  String currentPath = server.uri();
  String currentQuery = buildRelayQueryString();
  for (unsigned int i = 0; i < sizeof(relayHosts) / sizeof(relayHosts[0]); i++) {
    relayRequestToHost(relayHosts[i], currentPath, currentQuery);
  }
}

void relayLastCommand() {
  if (!repeatEnabled) {
    return;
  }
  if (lastRelayPath.length() == 0) {
    Serial.println("relayLastCommand: no last command stored");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("relayLastCommand: WiFi not connected");
    return;
  }
  Serial.print("relayLastCommand: repeating ");
  Serial.print(lastRelayPath);
  Serial.print(" ");
  Serial.println(lastRelayQuery);
  for (unsigned int i = 0; i < sizeof(relayHosts) / sizeof(relayHosts[0]); i++) {
    relayRequestToHost(relayHosts[i], lastRelayPath, lastRelayQuery);
  }
}

void handleRepeat() {
  if (lastRelayPath.length() == 0) {
    server.send(400, "text/plain", "No last command to repeat");
    return;
  }
  repeatEnabled = true;
  lastRepeatTime = millis();
  server.send(200, "text/plain", "Repeat relay started");
  Serial.println("Repeat relay started");
}

void handleStopRelaying() {
  repeatEnabled = false;
  server.send(200, "text/plain", "Repeat relay stopped");
  Serial.println("Repeat relay stopped");
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
// Shared Halloween palette. Orange has no blue and plenty of green so it reads
// as a deep pumpkin orange on WS2812B rather than drifting towards pink.
const CRGB HALLOWEEN_ORANGE = CRGB(255, 75, 0);
const CRGB HALLOWEEN_PURPLE = CRGB(120, 0, 180);
const CRGB HALLOWEEN_GREEN = CRGB(0, 255, 40);

CRGB randomHalloweenColor() {
  switch (random8(4)) {
    case 0:
    case 1: return HALLOWEEN_ORANGE;
    case 2: return HALLOWEEN_PURPLE;
    default: return HALLOWEEN_GREEN;
  }
}

// Halloween 0: "Witch comets" - coloured comets with fading tails race up and
// down the strip, bouncing off the ends and changing colour/speed at random
void showHalloween0() {
  struct Comet {
    int32_t pos;  // 8.8 fixed-point LED index
    int16_t vel;  // 8.8 fixed-point LEDs per frame
    CRGB color;
  };
  const int numComets = 5;
  static Comet comets[numComets];
  static bool initialized = false;
  const int32_t maxPos = (int32_t)(NUM_LEDS - 1) << 8;

  if (!initialized) {
    for (Comet& c : comets) {
      c.pos = (int32_t)random16(NUM_LEDS) << 8;
      c.vel = (int16_t)random16(120, 700) * (random8(2) ? 1 : -1);
      c.color = randomHalloweenColor();
    }
    initialized = true;
  }

  fadeToBlackBy(leds, NUM_LEDS, 70);
  for (Comet& c : comets) {
    c.pos += c.vel;
    if (c.pos <= 0 || c.pos >= maxPos) {
      c.pos = c.pos <= 0 ? 0 : maxPos;
      int16_t dir = c.vel > 0 ? -1 : 1;
      if (random8(3) == 0) {
        c.color = randomHalloweenColor();
        c.vel = dir * (int16_t)random16(120, 700);
      } else {
        c.vel = dir * abs(c.vel);
      }
    }
    int head = c.pos >> 8;
    leds[head] = c.color;
    // Anti-alias the leading edge so slow comets glide instead of stepping
    int next = head + (c.vel > 0 ? 1 : -1);
    if (next >= 0 && next < NUM_LEDS) {
      CRGB tip = c.color;
      tip.nscale8(c.vel > 0 ? (c.pos & 0xFF) : 255 - (c.pos & 0xFF));
      leds[next] |= tip;
    }
  }
}

// Halloween 1: Orange base, purple "ghost" pulses moving along the strip
void showHalloween1() {
  static uint16_t ghostPos = 0;
  fill_solid(leds, NUM_LEDS, HALLOWEEN_ORANGE);
  for (int i = 0; i < 5; i++) {
    int pos = (ghostPos + i * (NUM_LEDS / 5)) % NUM_LEDS;
    for (int j = -2; j <= 2; j++) {
      int idx = pos + j;
      if (idx >= 0 && idx < NUM_LEDS) {
        leds[idx] = HALLOWEEN_PURPLE; // purple ghost
      }
    }
  }
  ghostPos = (ghostPos + 1) % NUM_LEDS;
}

// Halloween 2: Purple base, random orange and green "pumpkin" flashes
void showHalloween2() {
  fill_solid(leds, NUM_LEDS, HALLOWEEN_PURPLE);
  for (int i = 0; i < NUM_LEDS / 12; i++) {
    int idx = random(NUM_LEDS);
    if (random8(2) == 0) {
      leds[idx] = HALLOWEEN_ORANGE;
    } else {
      leds[idx] = HALLOWEEN_GREEN;
    }
  }
}

// Halloween 3: "Haunted fog" - drifting purple fog with pairs of glowing eyes
// that open, blink and close at random, broken up by flickering lightning
void showHalloween3() {
  struct Eyes {
    int pos;
    uint8_t age;
    uint8_t life; // 0 = inactive
    CRGB color;
  };
  const int numEyes = 4;
  const int eyeGap = 3;
  static Eyes eyes[numEyes];
  static uint8_t strikeFrames = 0;

  // Two sine waves drifting in opposite directions make slowly shifting fog
  // banks. (FastLED's inoise8 crashes on ESP8266: it byte-reads a PROGMEM table.)
  uint32_t t = millis();
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t a = sin8((uint8_t)(i * 7 + t / 12));
    uint8_t b = sin8((uint8_t)(i * 11 - t / 17));
    uint8_t fog = qsub8((a + b) / 2, 100); // dark gaps between fog banks
    leds[i] = HALLOWEEN_PURPLE;
    leds[i].nscale8(qadd8(fog, fog >> 1));
  }

  for (Eyes& e : eyes) {
    if (e.life == 0) {
      if (random8(60) == 0) {
        e.pos = random16(NUM_LEDS - eyeGap);
        e.age = 0;
        e.life = random8(60, 200);
        e.color = random8(3) == 0 ? HALLOWEEN_GREEN : HALLOWEEN_ORANGE;
      }
      continue;
    }
    uint8_t level = 255;
    if (e.age < 8) level = e.age * 32;                         // opening
    else if (e.life - e.age < 8) level = (e.life - e.age) * 32; // closing
    else if (e.age % 45 < 3) level = 0;                         // blink
    CRGB c = e.color;
    c.nscale8(level);
    leds[e.pos] = c;
    leds[e.pos + eyeGap] = c;
    if (++e.age >= e.life) e.life = 0;
  }

  if (strikeFrames == 0 && random8() < 2) {
    strikeFrames = random8(6, 14);
  }
  if (strikeFrames > 0) {
    strikeFrames--;
    if (random8(3) != 0) {
      int len = random16(NUM_LEDS / 4, NUM_LEDS + 1);
      int start = random16(NUM_LEDS - len + 1);
      fill_solid(leds + start, len, random8(2) ? CRGB::White : CRGB(180, 140, 255));
    }
  }
}

// Halloween 4: "Cauldron shuffle" - the strip repaints in a new Halloween colour
// LED by LED, either in a random order or as a fast sweep (up, down or from the
// centre out). Each newly lit LED sparks white-hot before settling.
void showHalloween4() {
  static uint16_t order[NUM_LEDS];
  static CRGB target[NUM_LEDS];
  static int step = NUM_LEDS; // NUM_LEDS means the current transition is done
  static int rate = 1;
  static uint8_t hold = 0;
  static CRGB currentColor = CRGB::Black;

  if (step < NUM_LEDS) {
    for (int n = 0; n < rate && step < NUM_LEDS; n++, step++) {
      int idx = order[step];
      target[idx] = currentColor;
      if (currentColor) leds[idx] = CRGB(255, 220, 160); // spark
    }
    if (step >= NUM_LEDS) hold = random8(10, 45);
  } else if (hold > 0) {
    hold--;
  } else {
    CRGB next;
    do {
      next = CRGB::Black;
      if (random8(5) != 0) next = randomHalloweenColor();
    } while (next == currentColor);
    currentColor = next;

    uint8_t pattern = random8(6); // 0-2 shuffle, 3 up, 4 down, 5 centre out
    const int mid = NUM_LEDS / 2;
    for (int k = 0; k < NUM_LEDS; k++) {
      if (pattern == 4) order[k] = NUM_LEDS - 1 - k;
      else if (pattern == 5) order[k] = (k & 1) ? mid - (k + 1) / 2 : mid + k / 2;
      else order[k] = k;
    }
    if (pattern <= 2) {
      for (int i = NUM_LEDS - 1; i > 0; i--) {
        int j = random16(i + 1);
        uint16_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
      }
    }
    rate = NUM_LEDS / random8(10, 40);
    if (rate < 1) rate = 1;
    step = 0;
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    nblend(leds[i], target[i], 50);
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
  server.on("/repeat", HTTP_GET, handleRepeat);
  server.on("/stopRelaying", HTTP_GET, handleStopRelaying);
  // Add more endpoints here as needed
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  unsigned long now = millis();
  if (repeatEnabled && now - lastRepeatTime >= repeatIntervalMs) {
    lastRepeatTime = now;
    relayLastCommand();
  }
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
static WiFiEventHandler wifiDisconnectHandler;

void setup_wifi() {
  delay(10);
  Serial.println("Connecting to WiFi...");
  WiFi.persistent(false); // don't wear out flash / avoid stale stored config
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);  // clear any previous connection/static config
  delay(100);
  // Print exact reason on every disconnect (e.g. 15=4-way handshake timeout/wrong
  // password, 202=auth fail, 2xx=PMF/WPA3 mismatch)
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(
      [](const WiFiEventStationModeDisconnected& evt) {
        Serial.print(" [disconnect reason=");
        Serial.print(evt.reason);
        Serial.print("]");
      });
  if (useDHCP) {
    // Force DHCP by clearing any static config
    WiFi.config(0U, 0U, 0U);
  } else if (local_IP.fromString(STATIC_IP) && gateway.fromString(GATEWAY_IP) &&
             subnet.fromString(SUBNET_MASK)) {
    WiFi.config(local_IP, gateway, subnet);
  } else {
    Serial.println("Invalid static IP config in env_secrets.ini, falling back to DHCP");
    WiFi.config(0U, 0U, 0U);
  }
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (++attempts % 20 == 0) {
      Serial.print(" status=");
      Serial.println(WiFi.status()); // 1=no SSID found,4=auth/connect fail,6=disconnected
    }
    if (attempts == 40) {
      // Scan and report visible networks to help diagnose (band/SSID issues)
      Serial.println("\nStill not connected. Scanning for networks...");
      int n = WiFi.scanNetworks();
      for (int i = 0; i < n; i++) {
        Serial.printf("  %s (RSSI %d)%s\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i),
                      WiFi.encryptionType(i) == ENC_TYPE_NONE ? " [open]" : "");
      }
      WiFi.scanDelete();
      Serial.print("Retrying begin() for SSID: ");
      Serial.println(ssid);
      WiFi.begin(ssid, password);
    }
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  // Change status lights to green
  showBootStatus(CRGB::Green);
  FastLED.show();
}
