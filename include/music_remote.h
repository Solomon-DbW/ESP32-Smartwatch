#pragma once

#include "config.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLEAdvertising.h>

#define BLE_DEVICE_NAME "DIY Watch"

#define AMS_SERVICE_UUID        "89D3502B-0F36-433A-8EF4-C502AD55F8DC"
#define AMS_ENTITY_UPDATE_UUID  "2F7CABCE-808D-411F-9A0C-BB92BA96C102"

enum AmsStatus { AMS_IDLE, AMS_WAITING, AMS_CONNECTING, AMS_READY, AMS_FAILED };

struct NowPlaying {
  char     title[80];
  char     artist[64];
  float    duration;
  float    elapsed;
  float    rate;
  int      state;
  uint32_t elapsedAtMs;
  bool     haveTrack;
  bool     havePlayback;
};

extern volatile bool bleConnected;
extern volatile uint32_t bleConnectedAt;
extern volatile int amsStatus;
extern volatile uint32_t amsStartedAt;
extern volatile int amsTries;
extern portMUX_TYPE npMux;
extern NowPlaying np;
extern BLEClient* amsClient;

// HID consumer-control key bitmasks (match mediaReportMap in music_remote.cpp)
constexpr uint8_t KEY_PLAY_PAUSE = 1 << 0;
constexpr uint8_t KEY_NEXT       = 1 << 1;
constexpr uint8_t KEY_PREV       = 1 << 2;
constexpr uint8_t KEY_VOL_UP     = 1 << 3;
constexpr uint8_t KEY_VOL_DOWN   = 1 << 4;

constexpr int MUSIC_COUNT = 5;
extern const uint8_t MUSIC_MASKS[MUSIC_COUNT];
extern const char* const MUSIC_NAMES[MUSIC_COUNT];
extern int musicSel;
extern int musicFlashIdx;
extern uint32_t musicFlashAt;
extern bool musicFlashOk;

void clearNowPlaying();
void asciiFold(const char* in, char* out, size_t outSize);
void onEntityUpdate(BLERemoteCharacteristic*, uint8_t* data, size_t len, bool);
bool amsConnectOnce();
void amsTask(void*);
void addAmsSolicitation(BLEAdvertisementData& d);
void startBle();
bool sendMediaKey(uint8_t mask);
void musicSend(int idx);
