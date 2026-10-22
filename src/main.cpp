// DIY smartwatch application coordinator.
// Hardware wiring and button behaviour are documented in Wiring/.

#include <Arduino.h>

#include "AppConfig.h"
#include "Buttons.h"
#include "Stopwatch.h"
#include "Clock.h"
#include "Display.h"
#include "MusicRemote.h"

static Screen screen = SCREEN_ANALOGUE;
static bool screenOn = true;
static bool needRedraw = true;
static uint32_t lastActivity = 0;

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
