// DIY smartwatch application coordinator.
// Hardware wiring and button behaviour are documented in Wiring/.

#include <Arduino.h>

#include "AppConfig.h"
#include "Buttons.h"
#include "Stopwatch.h"
#include "Clock.h"
#include "Display.h"
#include "MusicRemote.h"

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
volatile uint32_t  bleConnectedAt = 0;
BLEAddress         phoneAddress("00:00:00:00:00:00");

// ---- Apple Media Service (AMS): now-playing info from the iPhone ----------
// The iPhone hosts AMS; the watch connects back to it as a GATT client over
// the same Bluetooth link and subscribes to track / playback updates.

#define AMS_SERVICE_UUID        "89D3502B-0F36-433A-8EF4-C502AD55F8DC"
#define AMS_ENTITY_UPDATE_UUID  "2F7CABCE-808D-411F-9A0C-BB92BA96C102"

// Same service UUID in on-air (little-endian) byte order. Advertising it as
// a "service solicitation" is what makes iOS expose AMS to the watch.
static const uint8_t AMS_SOLICIT_UUID_LE[16] = {
  0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E,
  0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89};

const uint8_t ENTITY_PLAYER = 0;
const uint8_t ENTITY_TRACK = 2;
const uint8_t PLAYER_ATTR_PLAYBACK_INFO = 1;  // "state,rate,elapsedSeconds"
const uint8_t TRACK_ATTR_ARTIST = 0;
const uint8_t TRACK_ATTR_TITLE = 2;
const uint8_t TRACK_ATTR_DURATION = 3;

enum AmsStatus { AMS_IDLE, AMS_WAITING, AMS_CONNECTING, AMS_READY, AMS_FAILED };
volatile int      amsStatus = AMS_IDLE;
volatile uint32_t amsStartedAt = 0;
volatile int      amsTries = 0;
BLEClient*        amsClient = nullptr;

struct NowPlaying {
  char     title[80];
  char     artist[64];
  float    duration;       // seconds
  float    elapsed;        // seconds, as of elapsedAtMs
  float    rate;           // 1.0 while playing
  int      state;          // 0 paused, 1 playing, 2 rewinding, 3 fast-forwarding
  uint32_t elapsedAtMs;
  bool     haveTrack;
  bool     havePlayback;
};

NowPlaying np = {};                              // written by BLE callbacks
portMUX_TYPE npMux = portMUX_INITIALIZER_UNLOCKED;  // guards np

static void clearNowPlaying() {
  portENTER_CRITICAL(&npMux);
  memset(&np, 0, sizeof(np));
  portEXIT_CRITICAL(&npMux);
}

// Titles arrive as UTF-8; the display fonts are ASCII-only. Turn common
// accented letters and punctuation into plain equivalents, others into '?'.
static void asciiFold(const char* in, char* out, size_t outSize) {
  static const char latin1[] =  // U+00C0 .. U+00FF
      "AAAAAAACEEEEIIIIDNOOOOO*OUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
  size_t o = 0;
  const uint8_t* p = (const uint8_t*)in;
  while (*p && o + 4 < outSize) {
    uint8_t c = *p;
    if (c < 0x80) {
      out[o++] = (c < 32) ? ' ' : (char)c;
      p++;
    } else if (c == 0xC3 && p[1] >= 0x80 && p[1] <= 0xBF) {
      out[o++] = latin1[p[1] - 0x80];
      p += 2;
    } else if (c == 0xE2 && p[1] == 0x80 && p[2] != 0) {
      uint8_t d = p[2];
      if (d == 0x98 || d == 0x99) out[o++] = '\'';
      else if (d == 0x9C || d == 0x9D) out[o++] = '"';
      else if (d == 0x93 || d == 0x94) out[o++] = '-';
      else if (d == 0xA6) { out[o++] = '.'; out[o++] = '.'; out[o++] = '.'; }
      else out[o++] = '?';
      p += 3;
    } else {
      out[o++] = '?';
      do { p++; } while ((*p & 0xC0) == 0x80);  // skip rest of the character
    }
  }
  out[o] = 0;
}

// Notification from the iPhone: [entity, attribute, flags, value...]
static void onEntityUpdate(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
  if (len < 3) return;
  uint8_t entity = data[0];
  uint8_t attr = data[1];

  char value[96];
  size_t n = len - 3;
  if (n > sizeof(value) - 1) n = sizeof(value) - 1;
  memcpy(value, data + 3, n);
  value[n] = 0;

  if (entity == ENTITY_PLAYER && attr == PLAYER_ATTR_PLAYBACK_INFO) {
    int state = 0;
    float rate = 0, elapsed = 0;
    if (sscanf(value, "%d,%f,%f", &state, &rate, &elapsed) >= 1) {
      portENTER_CRITICAL(&npMux);
      np.state = state;
      np.rate = rate;
      np.elapsed = elapsed;
      np.elapsedAtMs = millis();
      np.havePlayback = true;
      portEXIT_CRITICAL(&npMux);
    }
  } else if (entity == ENTITY_TRACK) {
    char clean[96];
    asciiFold(value, clean, sizeof(clean));
    portENTER_CRITICAL(&npMux);
    if (attr == TRACK_ATTR_TITLE) {
      strlcpy(np.title, clean, sizeof(np.title));
      np.haveTrack = (np.title[0] != 0);
    } else if (attr == TRACK_ATTR_ARTIST) {
      strlcpy(np.artist, clean, sizeof(np.artist));
    } else if (attr == TRACK_ATTR_DURATION) {
      np.duration = atof(value);
    }
    portEXIT_CRITICAL(&npMux);
  }
}

// One attempt to connect back to the iPhone's AMS and subscribe to updates.
static bool amsConnectOnce() {
  if (!amsClient) amsClient = BLEDevice::createClient();

  if (!amsClient->isConnected()) {
    Serial.println("AMS: connecting back to phone...");
    if (!amsClient->connect(phoneAddress, BLE_ADDR_TYPE_RANDOM)) {
      if (!bleConnected) return false;
      if (!amsClient->connect(phoneAddress, BLE_ADDR_TYPE_PUBLIC)) {
        Serial.println("AMS: connect failed");
        return false;
      }
    }
  }

  // Re-run service discovery each attempt (iOS may expose AMS only after bonding)
  amsClient->getServices();
  BLERemoteService* svc = amsClient->getService(BLEUUID(AMS_SERVICE_UUID));
  if (!svc) {
    Serial.println("AMS: service not found");
    return false;
  }
  BLERemoteCharacteristic* upd = svc->getCharacteristic(BLEUUID(AMS_ENTITY_UPDATE_UUID));
  if (!upd) {
    Serial.println("AMS: entity update characteristic not found");
    return false;
  }

  // Order matters: subscribe to notifications first, then say what we want.
  upd->registerForNotify(onEntityUpdate);
  const uint8_t wantPlayer[] = {ENTITY_PLAYER, PLAYER_ATTR_PLAYBACK_INFO};
  const uint8_t wantTrack[] = {ENTITY_TRACK, TRACK_ATTR_ARTIST, TRACK_ATTR_TITLE,
                               TRACK_ATTR_DURATION};
  upd->writeValue((uint8_t*)wantPlayer, sizeof(wantPlayer), true);
  upd->writeValue((uint8_t*)wantTrack, sizeof(wantTrack), true);
  Serial.println("AMS: ready");
  return true;
}

// Runs in its own task so a slow or stuck Bluetooth call never freezes the UI.
static void amsTask(void*) {
  for (;;) {
    if (!bleConnected || amsStatus == AMS_READY) {
      vTaskDelay(pdMS_TO_TICKS(400));
      continue;
    }
    // Give pairing / link encryption a few seconds to settle after connecting
    if (millis() - bleConnectedAt < 3000) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    amsStatus = AMS_CONNECTING;
    amsStartedAt = millis();
    bool ok = amsConnectOnce();
    if (!bleConnected) continue;  // link dropped meanwhile; callback reset the state

    if (ok) {
      amsTries = 0;
      amsStatus = AMS_READY;
    } else {
      amsTries++;
      amsStatus = (amsTries >= 3) ? AMS_FAILED : AMS_WAITING;
      vTaskDelay(pdMS_TO_TICKS(amsTries >= 3 ? 15000 : 5000));
    }
  }
}

class BleServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
    phoneAddress = BLEAddress(param->connect.remote_bda);
    bleConnectedAt = millis();
    amsTries = 0;
    amsStatus = AMS_WAITING;
    bleConnected = true;
  }
  void onDisconnect(BLEServer*) override {
    bleConnected = false;
    amsStatus = AMS_IDLE;
    clearNowPlaying();
    BLEDevice::startAdvertising();  // stay discoverable so the iPhone reconnects
  }
};

static void addAmsSolicitation(BLEAdvertisementData& d) {
  std::string data;
  data.push_back((char)17);    // length: 1 type byte + 16 UUID bytes
  data.push_back((char)0x15);  // type: list of 128-bit service solicitation UUIDs
  data.append((const char*)AMS_SOLICIT_UUID_LE, 16);
  d.addData(data);
}

static void startBle() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEDevice::setMTU(185);  // room for longer song titles in one notification
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

  // Advertising packet: flags, appearance, HID service.
  // Scan response: name + the AMS solicitation (too big to fit in one packet).
  BLEAdvertisementData advData;
  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setAppearance(0x03C0);  // generic HID device
  advData.setCompleteServices(bleHid->hidService()->getUUID());
  BLEAdvertisementData scanData;
  scanData.setName(BLE_DEVICE_NAME);
  addAmsSolicitation(scanData);

  BLEAdvertising* adv = bleServer->getAdvertising();
  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanData);
  adv->start();

  bleHid->setBatteryLevel(100);

  xTaskCreate(amsTask, "ams", 8192, nullptr, 1, nullptr);
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

// One control icon on the Music screen.
// playState: 1 = playing (show pause), 0 = paused (show play), -1 = unknown.
static void drawMusicIcon(int idx, int x, int y, bool selected, bool flashing,
                          int playState) {
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
      if (playState == 1) {          // playing: offer pause
        spr.fillRect(x - 6, y - 8, 4, 17, c);
        spr.fillRect(x + 3, y - 8, 4, 17, c);
      } else if (playState == 0) {   // paused: offer play
        spr.fillTriangle(x - 6, y - 9, x - 6, y + 9, x + 8, y, c);
      } else {                       // unknown: show both
        spr.fillTriangle(x - 10, y - 8, x - 10, y + 8, x, y, c);
        spr.fillRect(x + 4, y - 8, 3, 17, c);
        spr.fillRect(x + 9, y - 8, 3, 17, c);
      }
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

// Shorten text with "..." until it fits in maxW pixels (out must hold >= 100 bytes)
static void fitText(const char* src, int font, int maxW, char* out, size_t outSize) {
  strlcpy(out, src, outSize);
  if (spr.textWidth(out, font) <= maxW) return;
  char tmp[104];
  size_t n = strlen(out);
  while (n > 1) {
    n--;
    out[n] = 0;
    snprintf(tmp, sizeof(tmp), "%s...", out);
    if (spr.textWidth(tmp, font) <= maxW) {
      strlcpy(out, tmp, outSize);
      return;
    }
  }
}

static void drawMusic() {
  NowPlaying s;
  portENTER_CRITICAL(&npMux);
  s = np;
  portEXIT_CRITICAL(&npMux);

  bool linked = bleConnected;
  int st = amsStatus;
  if (st == AMS_CONNECTING && millis() - amsStartedAt > 25000) st = AMS_FAILED;
  bool showTrack = linked && s.haveTrack;
  int playState = (showTrack && s.havePlayback) ? (s.state == 1 ? 1 : 0) : -1;

  spr.fillSprite(TFT_BLACK);
  spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
  spr.setTextDatum(MC_DATUM);

  // Header: connection / playback state
  const char* head;
  uint16_t headColor;
  if (!linked)               { head = "NOT CONNECTED"; headColor = TFT_ORANGE; }
  else if (playState == 1)   { head = "PLAYING";       headColor = TFT_GREEN; }
  else if (playState == 0)   { head = "PAUSED";        headColor = TFT_YELLOW; }
  else                       { head = "MUSIC";         headColor = TFT_CYAN; }
  spr.setTextColor(headColor, TFT_BLACK);
  spr.drawString(head, CX, 36, 2);

  char buf[100];
  if (!linked) {
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString("Pair \"DIY Watch\" in", CX, 72, 2);
    spr.drawString("iPhone Bluetooth settings", CX, 92, 2);
  } else if (showTrack) {
    // Title: big if it fits, otherwise smaller and shortened
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    int tf = (spr.textWidth(s.title, 4) <= 200) ? 4 : 2;
    fitText(s.title, tf, 200, buf, sizeof(buf));
    spr.drawString(buf, CX, 66, tf);

    // Artist
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    fitText(s.artist, 2, 200, buf, sizeof(buf));
    spr.drawString(buf, CX, 94, 2);

    // Progress bar and times
    float el = s.elapsed;
    if (s.state == 1) el += s.rate * (millis() - s.elapsedAtMs) / 1000.0f;
    if (s.duration > 0 && el > s.duration) el = s.duration;
    if (el < 0) el = 0;

    const int barX = 50, barW = 140, barY = 112;
    spr.fillRect(barX, barY, barW, 4, TFT_DARKGREY);
    if (s.duration > 0) {
      int w = (int)(barW * (el / s.duration));
      if (w > barW) w = barW;
      spr.fillRect(barX, barY, w, 4, playState == 1 ? TFT_GREEN : TFT_YELLOW);
    }
    int e = (int)el, d = (int)s.duration;
    if (d > 0) snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d", e / 60, e % 60, d / 60, d % 60);
    else       snprintf(buf, sizeof(buf), "%d:%02d", e / 60, e % 60);
    spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
    spr.drawString(buf, CX, 126, 2);
  } else {
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    if (st == AMS_READY) {
      spr.drawString("Nothing playing", CX, 72, 2);
      spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
      spr.drawString("Start music on iPhone", CX, 94, 2);
    } else if (st == AMS_FAILED) {
      spr.drawString("Controls only", CX, 72, 2);
      spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
      spr.drawString("Track info unavailable", CX, 94, 2);
    } else {
      spr.drawString("Loading track info...", CX, 72, 2);
    }
  }

  // Control icons
  uint32_t now = millis();
  for (int i = 0; i < MUSIC_COUNT; i++) {
    bool flashing = (i == musicFlashIdx) && (now - musicFlashAt < 250);
    drawMusicIcon(i, 40 + i * 40, 156, i == musicSel, flashing, playState);
  }

  const char* label = MUSIC_NAMES[musicSel];
  if (musicSel == 2 && playState == 1) label = "Pause";
  else if (musicSel == 2 && playState == 0) label = "Play";
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(label, CX, 186, 2);

  spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
  spr.drawString("A: send  B: select", CX, 206, 2);
  spr.drawString("hold B: exit", CX, 222, 2);

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

  buttonsBegin();
  stopwatchBegin();

  if (!displayBegin()) {
    displayStatus("Sprite alloc failed");
    while (true) delay(1000);
  }

  displayStatus("Starting...");
  clockBegin();

  displayStatus("Starting Bluetooth...");
  musicRemoteBegin();

  lastActivity = millis();
  screenOn = true;
  needRedraw = true;
}

void loop() {
  const uint32_t now = millis();

  BtnEvent evA = updateButton(btnA);
  BtnEvent evB = updateButton(btnB);

  // Wake on any press; consume that press so waking cannot trigger an action.
  bool wokeNow = false;
  if ((evA == EV_DOWN || evB == EV_DOWN) && !screenOn) {
    screenOn = true;
    wokeNow = true;
    needRedraw = true;
    displaySetBacklight(true);
    if (evA == EV_DOWN) btnA.consume = true;
    if (evB == EV_DOWN) btnB.consume = true;
  }

  if (evA != EV_NONE || evB != EV_NONE) {
    lastActivity = now;
  }

  // Actions only run if the screen was already on at the start of the press.
  if (screenOn && !wokeNow) {
    if (screen == SCREEN_MUSIC) {
      // Music: short B selects a control, short A sends it, long B exits.
      if (evB == EV_SHORT) {
        musicSelectNext();
        needRedraw = true;
      }
      if (evB == EV_LONG) {
        screen = static_cast<Screen>((screen + 1) % SCREEN_COUNT);
        needRedraw = true;
      }
      if (evA == EV_SHORT) {
        musicRemoteSend(musicSelected());
        needRedraw = true;
      }
    } else {
      if (evB == EV_SHORT) {
        screen = static_cast<Screen>((screen + 1) % SCREEN_COUNT);
        needRedraw = true;
      }
      if (screen == SCREEN_STOPWATCH) {
        if (evA == EV_SHORT) {
          stopwatchToggle();
          needRedraw = true;
        }
        if (evA == EV_LONG) {
          stopwatchReset();
          needRedraw = true;
        }
      }
    }
  }

  // Intentionally keeps the display on continuously
  screenOn = true;
  displaySetBacklight(true);

  /*
  const uint32_t timeout =
      (screen == SCREEN_STOPWATCH && swRunning)
          ? STOPWATCH_TIMEOUT_MS
          : SCREEN_TIMEOUT_MS;

  if (screenOn && now - lastActivity > timeout) {
    screenOn = false;
    displaySetBacklight(false);
  }
  */

  if (screenOn) {
    static int lastSec = -1;
    static uint32_t lastDraw = 0;

    if (screen == SCREEN_ANALOGUE || screen == SCREEN_DIGITAL) {
      const time_t nowT = time(nullptr);
      struct tm localTimeInfo;
      localtime_r(&nowT, &localTimeInfo);

      if (needRedraw || localTimeInfo.tm_sec != lastSec) {
        lastSec = localTimeInfo.tm_sec;
        if (screen == SCREEN_ANALOGUE) {
          drawClock(localTimeInfo);
        } else {
          drawDigitalClock(localTimeInfo);
        }
        needRedraw = false;
      }
    } else if (screen == SCREEN_STOPWATCH) {
      if (needRedraw || now - lastDraw >= 100) {
        lastDraw = now;
        drawStopwatch();
        needRedraw = false;
      }
    } else {  // SCREEN_MUSIC: refresh connection state and key feedback at 4 Hz.
      if (needRedraw || now - lastDraw >= 250) {
        lastDraw = now;
        drawMusic();
        needRedraw = false;
      }
    }
  }

  delay(10);
}

