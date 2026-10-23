#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "AppConfig.h"
#include "Clock.h"
#include "Display.h"
#include "secrets.h"

static bool timeSynced = false;

void clockBegin() {
  displayStatus("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("IP %s  GW %s  DNS %s  RSSI %d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str(),
                  WiFi.RSSI());

    displayStatus("Syncing time...");
    configTzTime(TZ_INFO, "time.cloudflare.com",
                 "pool.ntp.org", "time.google.com");

    struct tm t;
    for (int attempt = 0; attempt < 3 && !timeSynced; ++attempt) {
      timeSynced = getLocalTime(&t, 10000);
      Serial.printf("NTP attempt %d: %s\n",
                    attempt + 1, timeSynced ? "ok" : "timeout");
    }

    if (!timeSynced) {
      displayStatus("NTP sync failed");
      delay(2000);
    }
  } else {
    displayStatus("WiFi failed");
    delay(2000);
  }

  // Keep Wi-Fi disabled so it does not compete with Bluetooth.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool clockIsSynced() {
  return timeSynced;
}

bool clockGetLocalTime(struct tm& result) {
  if (!timeSynced) return false;
  return getLocalTime(&result, 0);
}
