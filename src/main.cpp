// DIY smartwatch - step 1: NTP-synced clock on a GC9A01 round display
// Wiring (ESP32-S3 -> display): 3V3->VCC, GND->GND, 12->SCL, 11->SDA,
// 10->CS, 9->DC, 8->RST, 7->BL

#include <Arduino.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <time.h>

// ---- EDIT THESE ----
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
// UK time with automatic BST/GMT switching
const char* TZ_INFO = "GMT0BST,M3.5.0/1,M10.5.0";
// --------------------

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);  // full-screen buffer in PSRAM, no flicker

const int CX = 120;
const int CY = 120;

static void drawHand(float angleDeg, int length, int width, uint16_t color) {
  float a = (angleDeg - 90.0f) * DEG_TO_RAD;
  int x = CX + (int)(cosf(a) * length);
  int y = CY + (int)(sinf(a) * length);
  spr.drawWideLine(CX, CY, x, y, width, color, TFT_BLACK);
}

static void drawFace() {
  // outer ring
  spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
  // hour ticks
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

  // date (top) and digital time (bottom)
  char buf[24];
  strftime(buf, sizeof(buf), "%a %d %b", &t);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(buf, CX, 70, 2);

  strftime(buf, sizeof(buf), "%H:%M", &t);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(buf, CX, 170, 4);

  // hands
  float secA = t.tm_sec * 6.0f;
  float minA = t.tm_min * 6.0f + t.tm_sec * 0.1f;
  float hrA  = (t.tm_hour % 12) * 30.0f + t.tm_min * 0.5f;
  drawHand(hrA, 55, 6, TFT_WHITE);
  drawHand(minA, 85, 4, TFT_WHITE);
  drawHand(secA, 95, 2, TFT_RED);
  spr.fillCircle(CX, CY, 6, TFT_RED);

  spr.pushSprite(0, 0);
}

static void status(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, CX, CY, 2);
  Serial.println(msg);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(0);
  status("Starting...");

  // Full-screen sprite: 240*240*2 bytes = ~115KB, lives in PSRAM
  spr.setColorDepth(16);
  spr.setAttribute(PSRAM_ENABLE, 1);
  if (!spr.createSprite(240, 240)) {
    status("Sprite alloc failed");
    while (true) delay(1000);
  }

  // Connect to Wi-Fi and sync time
  status("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Network diagnostics (visible in the serial monitor)
    Serial.printf("IP %s  GW %s  DNS %s  RSSI %d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.gatewayIP().toString().c_str(),
                  WiFi.dnsIP().toString().c_str(),
                  WiFi.RSSI());
    IPAddress ntpIp;
    bool dnsOk = WiFi.hostByName("pool.ntp.org", ntpIp);
    Serial.printf("DNS lookup pool.ntp.org: %s (%s)\n",
                  dnsOk ? "OK" : "FAILED", ntpIp.toString().c_str());

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
    // Radio off after sync: saves a lot of power
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    status("WiFi failed");
    delay(2000);
    // Falls back to 1970 time; check SSID/password and reboot
  }
}

void loop() {
  static int lastSec = -1;
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  if (t.tm_sec != lastSec) {
    lastSec = t.tm_sec;
    drawClock(t);
  }
  delay(20);
}
