// DIY smartwatch - screens: analogue, digital, stopwatch, music remote
//
// Display (ESP32-S3 -> GC9A01): 3V3->VCC, GND->GND, 12->SCL, 11->SDA,
//                               10->CS, 9->DC, 8->RST, 7->BL
// Buttons: GPIO 4 = button A, GPIO 5 = button B (other leg of each to GND)
//
// Controls:
//   B (short)  : next screen
//   Stopwatch  : A = start / stop, hold A = reset
//   Music      : B = choose control, A = send it, hold B = next screen
//
// Music remote: the watch appears to the iPhone as a Bluetooth media-key
// accessory called "Solomon's ESP32 Watch". Pair it once in Settings > Bluetooth.

#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
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

// ------------------------------------------------- music remote (BLE HID)

#define BLE_DEVICE_NAME "Solomon's ESP32 Watch"

// HID report: Consumer Control, report ID 1, one byte of 5 button bits.
static const uint8_t mediaReportMap[] = {
  0x05, 0x0C,  // Usage Page (Consumer)
  0x09, 0x01,  // Usage (Consumer Control)
  0xA1, 0x01,  // Collection (Application)
  0x85, 0x01,  //   Report ID (1)
  0x15, 0x00,  //   Logical Minimum (0)
  0x25, 0x01,  //   Logical Maximum (1)
  0x75, 0x01,  //   Report Size (1)
  0x95, 0x05,  //   Report Count (5)
  0x09, 0xCD,  //   Play/Pause          (bit 0)
  0x09, 0xB5,  //   Scan Next Track     (bit 1)
  0x09, 0xB6,  //   Scan Previous Track (bit 2)
  0x09, 0xE9,  //   Volume Increment    (bit 3)
  0x09, 0xEA,  //   Volume Decrement    (bit 4)
  0x81, 0x02,  //   Input (Data, Variable, Absolute)
  0x75, 0x03,  //   Report Size (3)
  0x95, 0x01,  //   Report Count (1)
  0x81, 0x03,  //   Input (Constant) - padding to a full byte
  0xC0         // End Collection
};

const uint8_t KEY_PLAY_PAUSE = 1 << 0;
const uint8_t KEY_NEXT       = 1 << 1;
const uint8_t KEY_PREV       = 1 << 2;
const uint8_t KEY_VOL_UP     = 1 << 3;
const uint8_t KEY_VOL_DOWN   = 1 << 4;

BLEServer*         bleServer = nullptr;
BLEHIDDevice*      bleHid = nullptr;
BLECharacteristic* mediaInput = nullptr;
volatile bool      bleConnected = false;

class BleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { bleConnected = true; }
  void onDisconnect(BLEServer*) override {
    bleConnected = false;
    BLEDevice::startAdvertising();  // stay discoverable so the iPhone reconnects
  }
};

static void startBle() {
  BLEDevice::init(BLE_DEVICE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BleServerCallbacks());

  bleHid = new BLEHIDDevice(bleServer);
  mediaInput = bleHid->inputReport(1);  // report ID 1
  bleHid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  bleHid->hidInfo(0x00, 0x01);

  // Bonding ("just works"), the setting iOS accepts for BLE keyboards
  BLESecurity* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND);

  bleHid->reportMap((uint8_t*)mediaReportMap, sizeof(mediaReportMap));
  bleHid->startServices();

  BLEAdvertising* adv = bleServer->getAdvertising();
  adv->setAppearance(0x03C0);  // generic HID device
  adv->addServiceUUID(bleHid->hidService()->getUUID());
  adv->setScanResponse(false);
  adv->start();

  bleHid->setBatteryLevel(100);
}

// Press and release one media key. Returns false if no iPhone is connected.
static bool sendMediaKey(uint8_t mask) {
  if (!bleConnected || !mediaInput) return false;
  uint8_t report = mask;
  mediaInput->setValue(&report, 1);
  mediaInput->notify();
  delay(20);
  report = 0;
  mediaInput->setValue(&report, 1);
  mediaInput->notify();
  delay(10);
  return true;
}

// Controls shown on the Music screen, left to right
const int MUSIC_COUNT = 5;
const uint8_t MUSIC_MASKS[MUSIC_COUNT] = {KEY_VOL_DOWN, KEY_PREV, KEY_PLAY_PAUSE,
                                          KEY_NEXT, KEY_VOL_UP};
const char* const MUSIC_NAMES[MUSIC_COUNT] = {"Volume down", "Previous",
                                              "Play / Pause", "Next", "Volume up"};
int      musicSel = 2;          // start on Play / Pause
int      musicFlashIdx = -1;
uint32_t musicFlashAt = 0;
bool     musicFlashOk = true;

static void musicSend(int idx) {
  musicFlashIdx = idx;
  musicFlashAt = millis();
  musicFlashOk = sendMediaKey(MUSIC_MASKS[idx]);
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

// One control icon on the Music screen
static void drawMusicIcon(int idx, int x, int y, bool selected, bool flashing) {
  if (flashing) spr.fillCircle(x, y, 16, musicFlashOk ? TFT_DARKGREEN : TFT_MAROON);
  if (selected) {
    spr.drawCircle(x, y, 18, TFT_YELLOW);
    spr.drawCircle(x, y, 19, TFT_YELLOW);
  }
  uint16_t c = TFT_WHITE;
  switch (idx) {
    case 0:  // volume down
      spr.setTextColor(c);
      spr.drawString("V-", x, y, 2);
      break;
    case 1:  // previous
      spr.fillTriangle(x + 7, y - 8, x + 7, y + 8, x - 5, y, c);
      spr.fillRect(x - 9, y - 8, 3, 17, c);
      break;
    case 2:  // play / pause
      spr.fillTriangle(x - 10, y - 8, x - 10, y + 8, x, y, c);
      spr.fillRect(x + 4, y - 8, 3, 17, c);
      spr.fillRect(x + 9, y - 8, 3, 17, c);
      break;
    case 3:  // next
      spr.fillTriangle(x - 7, y - 8, x - 7, y + 8, x + 5, y, c);
      spr.fillRect(x + 6, y - 8, 3, 17, c);
      break;
    case 4:  // volume up
      spr.setTextColor(c);
      spr.drawString("V+", x, y, 2);
      break;
  }
}

static void drawMusic() {
  spr.fillSprite(TFT_BLACK);
  spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
  spr.setTextDatum(MC_DATUM);

  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.drawString("MUSIC", CX, 45, 2);

  spr.setTextColor(bleConnected ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
  spr.drawString(bleConnected ? "iPhone connected" : "Not connected", CX, 68, 2);

  uint32_t now = millis();
  for (int i = 0; i < MUSIC_COUNT; i++) {
    bool flashing = (i == musicFlashIdx) && (now - musicFlashAt < 250);
    drawMusicIcon(i, 40 + i * 40, 120, i == musicSel, flashing);
  }

  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(MUSIC_NAMES[musicSel], CX, 156, 2);

  spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
  if (bleConnected) {
    spr.drawString("A: send  B: select", CX, 188, 2);
    spr.drawString("hold B: next screen", CX, 206, 2);
  } else {
    spr.drawString("Pair \"ESP32 Watch\" in", CX, 188, 2);
    spr.drawString("iPhone Bluetooth", CX, 206, 2);
    spr.drawString("hold B: exit", CX, 224, 2);
  }

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

enum Screen {
  SCREEN_ANALOGUE,
  SCREEN_DIGITAL,
  SCREEN_STOPWATCH,
  SCREEN_MUSIC,
  SCREEN_COUNT
};

Screen   screen = SCREEN_ANALOGUE;
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
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);  // keep the radio quiet so Bluetooth has it to itself
  }

  status("Starting Bluetooth...");
  startBle();

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
    if (screen == SCREEN_MUSIC) {
      // Music: B picks a control, A sends it, hold B leaves the screen
      if (evB == EV_SHORT) {
        musicSel = (musicSel + 1) % MUSIC_COUNT;
        needRedraw = true;
      }
      if (evB == EV_LONG) {
        screen = (Screen)((screen + 1) % SCREEN_COUNT);
        needRedraw = true;
      }
      if (evA == EV_SHORT) {
        musicSend(musicSel);
        needRedraw = true;
      }
    } else {
      if (evB == EV_SHORT) {
        screen = (Screen)((screen + 1) % SCREEN_COUNT);
        needRedraw = true;
      }
      if (screen == SCREEN_STOPWATCH) {
        if (evA == EV_SHORT) { stopwatchToggle(); needRedraw = true; }
        if (evA == EV_LONG)  { stopwatchReset();  needRedraw = true; }
      }
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
    } else if (screen == SCREEN_STOPWATCH) {
      if (needRedraw || now - lastDraw >= 100) {
        lastDraw = now;
        drawStopwatch();
        needRedraw = false;
      }
    } else {  // SCREEN_MUSIC (refreshes ~4x/s for connection status + key flash)
      if (needRedraw || now - lastDraw >= 250) {
        lastDraw = now;
        drawMusic();
        needRedraw = false;
      }
    }
  }

  delay(10);
}
