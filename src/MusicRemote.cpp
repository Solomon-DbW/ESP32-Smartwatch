#include <Arduino.h>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLEAdvertising.h>

#include "AppConfig.h"
#include "MusicRemote.h"

// BLE_DEVICE_NAME is defined in AppConfig.h

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

void musicRemoteBegin() {
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

const uint8_t MUSIC_MASKS[MUSIC_COUNT] = {KEY_VOL_DOWN, KEY_PREV, KEY_PLAY_PAUSE,
                                          KEY_NEXT, KEY_VOL_UP};
const char* const MUSIC_NAMES[MUSIC_COUNT] = {"Volume down", "Previous",
                                              "Play / Pause", "Next", "Volume up"};
int      musicSel = 2;          // start on Play / Pause
int      musicFlashIdx = -1;
uint32_t musicFlashAt = 0;
bool     musicFlashOk = true;


bool musicRemoteConnected() {
  return bleConnected;
}

bool musicRemoteSend(int index) {
  if (index < 0 || index >= MUSIC_COUNT) return false;
  musicFlashIdx = index;
  musicFlashAt = millis();
  musicFlashOk = sendMediaKey(MUSIC_MASKS[index]);
  return musicFlashOk;
}

int musicSelected() {
  return musicSel;
}

void musicSelectNext() {
  musicSel = (musicSel + 1) % MUSIC_COUNT;
}

MusicSnapshot musicRemoteSnapshot() {
  MusicSnapshot result = {};
  portENTER_CRITICAL(&npMux);
  strlcpy(result.title, np.title, sizeof(result.title));
  strlcpy(result.artist, np.artist, sizeof(result.artist));
  result.duration = np.duration;
  result.elapsed = np.elapsed;
  result.rate = np.rate;
  result.state = np.state;
  result.elapsedAtMs = np.elapsedAtMs;
  result.haveTrack = np.haveTrack;
  result.havePlayback = np.havePlayback;
  portEXIT_CRITICAL(&npMux);

  result.connected = bleConnected;
  result.amsStatus = amsStatus;
  result.amsStartedAt = amsStartedAt;
  result.flashIndex = musicFlashIdx;
  result.flashAt = musicFlashAt;
  result.flashOk = musicFlashOk;
  result.selected = musicSel;
  return result;
}
