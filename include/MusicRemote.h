#pragma once
#include <Arduino.h>

constexpr int MUSIC_COUNT = 5;

enum AmsStatus {
  AMS_IDLE,
  AMS_WAITING,
  AMS_CONNECTING,
  AMS_READY,
  AMS_FAILED
};

struct MusicSnapshot {
  char title[80];
  char artist[64];
  float duration;
  float elapsed;
  float rate;
  int state;
  uint32_t elapsedAtMs;
  bool haveTrack;
  bool havePlayback;
  bool connected;
  int amsStatus;
  uint32_t amsStartedAt;
  int flashIndex;
  uint32_t flashAt;
  bool flashOk;
  int selected;
};

extern const char* const MUSIC_NAMES[MUSIC_COUNT];

void musicRemoteBegin();
bool musicRemoteConnected();
bool musicRemoteSend(int index);
int musicSelected();
void musicSelectNext();
MusicSnapshot musicRemoteSnapshot();
