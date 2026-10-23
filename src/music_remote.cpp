#include "music_remote.h"

static const uint8_t mediaReportMap[] = {
  0x05, 0x0C,
  0x09, 0x01,
  0xA1, 0x01,
  0x85, 0x01,
  0x15, 0x00,
  0x25, 0x01,
  0x75, 0x01,
  0x95, 0x05,
  0x09, 0xCD,
  0x09, 0xB5,
  0x09, 0xB6,
  0x09, 0xE9,
  0x09, 0xEA,
  0x81, 0x02,
  0x75, 0x03,
  0x95, 0x01,
  0x81, 0x03,
  0xC0
};

const uint8_t MUSIC_MASKS[5] = { KEY_VOL_DOWN, KEY_PREV, KEY_PLAY_PAUSE, KEY_NEXT, KEY_VOL_UP };
const char* const MUSIC_NAMES[5] = { "Volume down", "Previous", "Play / Pause", "Next", "Volume up" };

int      musicSel = 2;
int      musicFlashIdx = -1;
uint32_t musicFlashAt = 0;
bool     musicFlashOk = true;

BLEServer*         bleServer = nullptr;
BLEHIDDevice*      bleHid = nullptr;
BLECharacteristic* mediaInput = nullptr;
volatile bool      bleConnected = false;
volatile uint32_t  bleConnectedAt = 0;
BLEAddress         phoneAddress("00:00:00:00:00:00");

const uint8_t AMS_SOLICIT_UUID_LE[16] = {
  0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E,
  0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89
};

const uint8_t ENTITY_PLAYER = 0;
const uint8_t ENTITY_TRACK = 2;
const uint8_t PLAYER_ATTR_PLAYBACK_INFO = 1;
const uint8_t TRACK_ATTR_ARTIST = 0;
const uint8_t TRACK_ATTR_TITLE = 2;
const uint8_t TRACK_ATTR_DURATION = 3;

volatile int      amsStatus = AMS_IDLE;
volatile uint32_t amsStartedAt = 0;
volatile int      amsTries = 0;
BLEClient*        amsClient = nullptr;

NowPlaying np = {};
portMUX_TYPE npMux = portMUX_INITIALIZER_UNLOCKED;

void clearNowPlaying() {
  portENTER_CRITICAL(&npMux);
  memset(&np, 0, sizeof(np));
  portEXIT_CRITICAL(&npMux);
}

void asciiFold(const char* in, char* out, size_t outSize) {
  static const char latin1[] =
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
      do { p++; } while ((*p & 0xC0) == 0x80);
    }
  }
  out[o] = 0;
}

void onEntityUpdate(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool) {
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

bool amsConnectOnce() {
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

  upd->registerForNotify(onEntityUpdate);

  const uint8_t wantPlayer[] = { ENTITY_PLAYER, PLAYER_ATTR_PLAYBACK_INFO };
  const uint8_t wantTrack[] = { ENTITY_TRACK, TRACK_ATTR_ARTIST, TRACK_ATTR_TITLE, TRACK_ATTR_DURATION };

  upd->writeValue((uint8_t*)wantPlayer, sizeof(wantPlayer), true);
  upd->writeValue((uint8_t*)wantTrack, sizeof(wantTrack), true);

  Serial.println("AMS: ready");
  return true;
}

void amsTask(void*) {
  for (;;) {
    if (!bleConnected || amsStatus == AMS_READY) {
      vTaskDelay(pdMS_TO_TICKS(400));
      continue;
    }

    if (millis() - bleConnectedAt < 3000) {
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    amsStatus = AMS_CONNECTING;
    amsStartedAt = millis();
    bool ok = amsConnectOnce();

    if (!bleConnected) continue;

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
    BLEDevice::startAdvertising();
  }
};

void addAmsSolicitation(BLEAdvertisementData& d) {
  std::string data;
  data.push_back((char)17);
  data.push_back((char)0x15);
  data.append((const char*)AMS_SOLICIT_UUID_LE, 16);
  d.addData(data);
}

void startBle() {
  BLEDevice::init(BLE_DEVICE_NAME);
  BLEDevice::setMTU(185);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new BleServerCallbacks());

  bleHid = new BLEHIDDevice(bleServer);
  mediaInput = bleHid->inputReport(1);
  bleHid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  bleHid->hidInfo(0x00, 0x01);

  BLESecurity* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND);

  bleHid->reportMap((uint8_t*)mediaReportMap, sizeof(mediaReportMap));
  bleHid->startServices();

  BLEAdvertisementData advData;
  advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advData.setAppearance(0x03C0);
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

bool sendMediaKey(uint8_t mask) {
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

void musicSend(int idx) {
  musicFlashIdx = idx;
  musicFlashAt = millis();
  musicFlashOk = sendMediaKey(MUSIC_MASKS[idx]);
}
