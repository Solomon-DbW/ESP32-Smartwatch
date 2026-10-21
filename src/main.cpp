#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <time.h>
#include "secrets.h"  // WIFI_SSID and WIFI_PASS live here

// UK time with automatic BST/GMT switching
const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";

const uint8_t  PIN_BTN_A = 4;
const uint8_t  PIN_BTN_B = 5;
const uint32_t DEBOUNCE_MS = 30;
const uint32_t LONG_PRESS_MS = 800;
const uint32_t SCREEN_TIMEOUT_MS = 5000;          // normal screen-off delay
const uint32_t STOPWATCH_TIMEOUT_MS = 15000;      // longer while timing

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);  // full-screen buffer in PSRAM

const int CX = 120;
const int CY = 120;

// ---------------------------------------------------------------- buttons

enum BtnEvent { EV_NONE, EV_DOWN, EV_SHORT, EV_LONG };

struct Button {
  uint8_t  pin;
  bool     lastRaw;
  bool     stable;
  uint32_t lastChange;
  uint32_t pressedAt;
  bool     longFired;
  bool     consume;   // swallow the rest of this press (used when it woke the screen)
};

Button btnA = {PIN_BTN_A, false, false, 0, 0, false, false};
Button btnB = {PIN_BTN_B, false, false, 0, 0, false, false};

// Call every loop. Returns at most one event per call:
//   EV_DOWN  - the moment the button is pressed
//   EV_SHORT - released after a short press
//   EV_LONG  - held past LONG_PRESS_MS (fires once, while still held)
static BtnEvent updateButton(Button& b) {
  uint32_t now = millis();
  bool raw = (digitalRead(b.pin) == LOW);  // pressed = LOW (pull-up)
  if (raw != b.lastRaw) {
    b.lastRaw = raw;
    b.lastChange = now;
  }

  BtnEvent ev = EV_NONE;
  if (now - b.lastChange >= DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (b.stable) {
      b.pressedAt = now;
      b.longFired = false;
      ev = EV_DOWN;
    } else {
      if (!b.longFired && !b.consume) ev = EV_SHORT;
      b.consume = false;
    }
  }

  if (ev == EV_NONE && b.stable && !b.longFired && !b.consume &&
      now - b.pressedAt >= LONG_PRESS_MS) {
    b.longFired = true;
    ev = EV_LONG;
  }
  return ev;
}

// ------------------------------------------------------------- stopwatch

bool     swRunning = false;
uint32_t swStartMs = 0;     // millis() when it was last started
uint32_t swBaseMs = 0;      // time accumulated before the current run

static uint32_t stopwatchElapsed() {
  return swRunning ? swBaseMs + (millis() - swStartMs) : swBaseMs;
}

static void stopwatchToggle() {
  if (swRunning) {
    swBaseMs += millis() - swStartMs;
    swRunning = false;
  } else {
    swStartMs = millis();
    swRunning = true;
  }
}

static void stopwatchReset() {
  swRunning = false;
  swBaseMs = 0;
}

// --------------------------------------------------------------- drawing

static void drawHand(float angleDeg, int length, int width, uint16_t color) {
  float a = (angleDeg - 90.0f) * DEG_TO_RAD;
  int x = CX + (int)(cosf(a) * length);
  int y = CY + (int)(sinf(a) * length);
  spr.drawWideLine(CX, CY, x, y, width, color, TFT_BLACK);
}

static void drawFace() {
  spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
  for (int i = 0; i < 12; i++) {
    float a = i * 30 * DEG_TO_RAD;
    int r1 = (i % 3 == 0) ? 98 : 106;
    int r2 = 114;
    spr.drawWideLine(CX + sinf(a) * r1, CY - cosf(a) * r1,
                     CX + sinf(a) * r2, CY - cosf(a) * r2,
                     (i % 3 == 0) ? 4 : 2, TFT_WHITE, TFT_BLACK);
  }
}

static void drawClock(const struct tm& t) {
  spr.fillSprite(TFT_BLACK);
  drawFace();

  char buf[24];
  strftime(buf, sizeof(buf), "%a %d %b", &t);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(buf, CX, 70, 2);

  strftime(buf, sizeof(buf), "%H:%M", &t);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, CX, 170, 4);

  float secA = t.tm_sec * 6.0f;
  float minA = t.tm_min * 6.0f + t.tm_sec * 0.1f;
  float hrA  = (t.tm_hour % 12) * 30.0f + t.tm_min * 0.5f;
  drawHand(hrA, 55, 6, TFT_WHITE);
  drawHand(minA, 85, 4, TFT_WHITE);
  drawHand(secA, 95, 2, TFT_RED);
  spr.fillCircle(CX, CY, 6, TFT_RED);

  spr.pushSprite(0, 0);
}

static void drawDigitalClock(const struct tm& t) {
  spr.fillSprite(TFT_BLACK);

  // Date
  char dateBuf[24];
  strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &t);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.drawString(dateBuf, CX, 65, 2);

  // Digital time
  char timeBuf[12];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &t);

  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(timeBuf, CX, 115, 4);

  // Seconds indicator
  spr.drawCircle(CX, 165, 5, TFT_GREEN);
  spr.fillCircle(CX, 165, 3, TFT_GREEN);

  spr.pushSprite(0, 0);
}

static void drawStopwatch() {
  uint32_t totalTenths = stopwatchElapsed() / 100;
  unsigned tenths = totalTenths % 10;
  unsigned secs = (totalTenths / 10) % 60;
  unsigned mins = (totalTenths / 600) % 100;

  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u.%u", mins, secs, tenths);

  spr.fillSprite(TFT_BLACK);
  spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.drawString("STOPWATCH", CX, 55, 2);

  spr.setTextColor(swRunning ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, CX, 120, 6);

  spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
  spr.drawString("A: start / stop", CX, 185, 2);
  spr.drawString("hold A: reset", CX, 205, 2);

  spr.pushSprite(0, 0);
}

static void status(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, CX, CY, 2);
  Serial.println(msg);
}

// ----------------------------------------------------------------- state

// enum Screen { SCREEN_WATCH, SCREEN_STOPWATCH, SCREEN_COUNT };

enum Screen {
  SCREEN_ANALOGUE,
  SCREEN_DIGITAL,
  SCREEN_STOPWATCH,
  SCREEN_COUNT
};

Screen screen = SCREEN_ANALOGUE;

// Screen   screen = SCREEN_WATCH;
bool     screenOn = true;
bool     needRedraw = true;
uint32_t lastActivity = 0;

// ----------------------------------------------------------------- setup

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_B, INPUT_PULLUP);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(0);
  status("Starting...");

  spr.setColorDepth(16);
  spr.setAttribute(PSRAM_ENABLE, 1);
  if (!spr.createSprite(240, 240)) {
    status("Sprite alloc failed");
    while (true) delay(1000);
  }

  status("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("IP %s  GW %s  DNS %s  RSSI %d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str(),
                  WiFi.RSSI());

    status("Syncing time...");
    configTzTime(TZ_INFO, "time.cloudflare.com", "pool.ntp.org", "time.google.com");
    struct tm t;
    bool synced = false;
    for (int i = 0; i < 3 && !synced; i++) {
      synced = getLocalTime(&t, 10000);
      Serial.printf("NTP attempt %d: %s\n", i + 1, synced ? "ok" : "timeout");
    }
    if (!synced) {
      status("NTP sync failed");
      delay(2000);
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    status("WiFi failed");
    delay(2000);
  }

  lastActivity = millis();
  screenOn = true;
  needRedraw = true;
}

// ------------------------------------------------------------------ loop

void loop() {
  uint32_t now = millis();

  BtnEvent evA = updateButton(btnA);
  BtnEvent evB = updateButton(btnB);

  // Wake on any press. The rest of that press is ignored, so waking
  // the screen never also triggers an action.
  bool wokeNow = false;
  if ((evA == EV_DOWN || evB == EV_DOWN) && !screenOn) {
    screenOn = true;
    wokeNow = true;
    needRedraw = true;
    digitalWrite(TFT_BL, HIGH);
    if (evA == EV_DOWN) btnA.consume = true;
    if (evB == EV_DOWN) btnB.consume = true;
  }
  if (evA != EV_NONE || evB != EV_NONE) lastActivity = now;

  // Actions (only when the screen was already on)
  if (screenOn && !wokeNow) {
    if (evB == EV_SHORT) {
      screen = (Screen)((screen + 1) % SCREEN_COUNT);
      needRedraw = true;
    }
    if (screen == SCREEN_STOPWATCH) {
      if (evA == EV_SHORT) { stopwatchToggle(); needRedraw = true; }
      if (evA == EV_LONG)  { stopwatchReset();  needRedraw = true; }
    }
  }

  // Keep the screen on continuously
  screenOn = true;
  digitalWrite(TFT_BL, HIGH);

  // // Screen timeout (longer while the stopwatch is running)
  // uint32_t timeout = (screen == SCREEN_STOPWATCH && swRunning)
  //                        ? STOPWATCH_TIMEOUT_MS
  //                        : SCREEN_TIMEOUT_MS;
  // if (screenOn && now - lastActivity > timeout) {
  //   screenOn = false;
  //   digitalWrite(TFT_BL, LOW);
  // }

  // Draw only while the screen is on
  if (screenOn) {
    static int lastSec = -1;
    static uint32_t lastDraw = 0;

  if (screen == SCREEN_ANALOGUE || screen == SCREEN_DIGITAL) {
    time_t nowT = time(nullptr);
    struct tm t;
    localtime_r(&nowT, &t);

    if (needRedraw || t.tm_sec != lastSec) {
      lastSec = t.tm_sec;

      if (screen == SCREEN_ANALOGUE) {
        drawClock(t);
      } else {
        drawDigitalClock(t);
      }

      needRedraw = false;
    }
  } else {
    if (needRedraw || now - lastDraw >= 100) {
      lastDraw = now;
      drawStopwatch();
      needRedraw = false;
    }
  }
  }

  delay(10);
}
