#include "config.h"
#include "buttons.h"
#include "stopwatch.h"
#include "music_remote.h"
#include "notifications.h"
#include "display.h"
#include "app_state.h"

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
    WiFi.mode(WIFI_OFF);
  }

  status("Starting Bluetooth...");
  startBle();

  lastActivity = millis();
  screenOn = true;
  needRedraw = true;
}

void loop() {
  uint32_t now = millis();

#if LOOP_DIAG
  static uint32_t lastLoopAt = 0;
  if (lastLoopAt && now - lastLoopAt > 100) {
    Serial.printf("[diag] loop stalled %u ms (ble=%d ams=%d screen=%d)\n",
                  (unsigned)(now - lastLoopAt), (int)bleConnected, (int)amsStatus, (int)screen);
  }
  lastLoopAt = now;
#endif

  BtnEvent evA = updateButton(btnA);
  BtnEvent evB = updateButton(btnB);

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

  if (screenOn && !wokeNow) {
    if (screen == SCREEN_MUSIC) {
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
    } else if (screen == SCREEN_NOTIFS) {
      if (notifDetail) {
        // full-message view
        if (evB == EV_SHORT) { notifPage++; needRedraw = true; }      // next page (wraps back to list)
        if (evA == EV_SHORT || evB == EV_LONG) { notifDetail = false; needRedraw = true; }
        if (evA == EV_LONG)  { notifDismissSelected(); needRedraw = true; }
      } else {
        if (evB == EV_SHORT) { notifNext(); needRedraw = true; }
        if (evB == EV_LONG) {
          screen = (Screen)((screen + 1) % SCREEN_COUNT);
          needRedraw = true;
        }
        if (evA == EV_SHORT) { notifOpenSelected(); needRedraw = true; }
        if (evA == EV_LONG)  { notifDismissSelected(); needRedraw = true; }
      }
    } else if (screen == SCREEN_FLASHLIGHT) {
      if (evA == EV_SHORT) { toggleFlashlight(); needRedraw = true; }
      if (evB == EV_SHORT) {
        flashlightOn = false;                       // never leave it lit on another screen
        screen = (Screen)((screen + 1) % SCREEN_COUNT);
        needRedraw = true;
      }
    } else {
      if (evB == EV_SHORT) {
        screen = (Screen)((screen + 1) % SCREEN_COUNT);
        needRedraw = true;
      }

      if (screen == SCREEN_QUADCLOCK && evA == EV_SHORT) {
        clock24h = !clock24h;                     // 12h <-> 24h
        needRedraw = true;
      }

      if (screen == SCREEN_STOPWATCH) {
        if (evA == EV_SHORT) { stopwatchToggle(); needRedraw = true; }
        if (evA == EV_LONG)  { stopwatchReset(); needRedraw = true; }
      }
    }
  }

#if SCREEN_SLEEP_ENABLED
  if (screenOn) {
    uint32_t timeout = (screen == SCREEN_STOPWATCH || screen == SCREEN_NOTIFS || screen == SCREEN_MUSIC)
                         ? STOPWATCH_TIMEOUT_MS : SCREEN_TIMEOUT_MS;
    if (now - lastActivity >= timeout) {
      screenOn = false;
      tft.fillScreen(TFT_BLACK);   // blank it even if the backlight can't be switched
      digitalWrite(TFT_BL, LOW);
    }
  }
#endif

#if LOOP_DIAG
  uint32_t drawStart = millis();
#endif
  if (screenOn) {
    static int lastSec = -1;
    static uint32_t lastDraw = 0;

    if (screen == SCREEN_ANALOGUE || screen == SCREEN_DIGITAL || screen == SCREEN_QUADCLOCK) {
      time_t nowT = time(nullptr);
      struct tm t;
      localtime_r(&nowT, &t);

      if (needRedraw || t.tm_sec != lastSec) {
        lastSec = t.tm_sec;

        if (screen == SCREEN_ANALOGUE) drawClock(t);
        else if (screen == SCREEN_QUADCLOCK) drawQuadClock(t);
        else drawDigitalClock(t);

        needRedraw = false;
      }
    } else if (screen == SCREEN_STOPWATCH) {
      if (needRedraw || now - lastDraw >= 100) {
        lastDraw = now;
        drawStopwatch();
        needRedraw = false;
      }
    } else if (screen == SCREEN_NOTIFS) {
      if (needRedraw || now - lastDraw >= 250) {
        lastDraw = now;
        drawNotifications();
        needRedraw = false;
      }
    } else if (screen == SCREEN_FLASHLIGHT) {
      if (needRedraw) {
        drawFlashlight();
        needRedraw = false;
      }
    } else {
      if (needRedraw || now - lastDraw >= 250) {
        lastDraw = now;
        drawMusic();
        needRedraw = false;
      }
    }
  }

#if LOOP_DIAG
  uint32_t drawMs = millis() - drawStart;
  if (drawMs > 60) Serial.printf("[diag] slow draw %u ms (screen=%d)\n", (unsigned)drawMs, (int)screen);
#endif

  delay(10);
}
