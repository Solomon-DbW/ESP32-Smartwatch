#include "display.h"

const int CX = 120;
const int CY = 120;

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

void drawHand(float angleDeg, int length, int width, uint16_t color) {
  float a = (angleDeg - 90.0f) * DEG_TO_RAD;
  int x = CX + (int)(cosf(a) * length);
  int y = CY + (int)(sinf(a) * length);
  spr.drawWideLine(CX, CY, x, y, width, color, TFT_BLACK);
}

void drawFace() {
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

void drawClock(const struct tm& t) {
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
  drawNotifBadge(205);

  float secA = t.tm_sec * 6.0f;
  float minA = t.tm_min * 6.0f + t.tm_sec * 0.1f;
  float hrA  = (t.tm_hour % 12) * 30.0f + t.tm_min * 0.5f;
  drawHand(hrA, 55, 6, TFT_WHITE);
  drawHand(minA, 85, 4, TFT_WHITE);
  drawHand(secA, 95, 2, TFT_RED);
  spr.fillCircle(CX, CY, 6, TFT_RED);

  spr.pushSprite(0, 0);
}

void drawDigitalClock(const struct tm& t) {
  spr.fillSprite(TFT_BLACK);

  char dateBuf[24];
  strftime(dateBuf, sizeof(dateBuf), "%a %d %b", &t);

  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  spr.drawString(dateBuf, CX, 65, 2);

  char timeBuf[12];
  strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &t);

  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString(timeBuf, CX, 115, 4);

  spr.drawCircle(CX, 165, 5, TFT_GREEN);
  spr.fillCircle(CX, 165, 3, TFT_GREEN);
  drawNotifBadge(200);

  spr.pushSprite(0, 0);
}

void drawStopwatch() {
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

void drawMusicIcon(int idx, int x, int y, bool selected, bool flashing, int playState) {
  if (flashing) spr.fillCircle(x, y, 16, musicFlashOk ? TFT_DARKGREEN : TFT_MAROON);
  if (selected) {
    spr.drawCircle(x, y, 18, TFT_YELLOW);
    spr.drawCircle(x, y, 19, TFT_YELLOW);
  }

  uint16_t c = TFT_WHITE;
  switch (idx) {
    case 0:
      spr.setTextColor(c);
      spr.drawString("V-", x, y, 2);
      break;
    case 1:
      spr.fillTriangle(x + 7, y - 8, x + 7, y + 8, x - 5, y, c);
      spr.fillRect(x - 9, y - 8, 3, 17, c);
      break;
    case 2:
      if (playState == 1) {
        spr.fillRect(x - 6, y - 8, 4, 17, c);
        spr.fillRect(x + 3, y - 8, 4, 17, c);
      } else if (playState == 0) {
        spr.fillTriangle(x - 6, y - 9, x - 6, y + 9, x + 8, y, c);
      } else {
        spr.fillTriangle(x - 10, y - 8, x - 10, y + 8, x, y, c);
        spr.fillRect(x + 4, y - 8, 3, 17, c);
        spr.fillRect(x + 9, y - 8, 3, 17, c);
      }
      break;
    case 3:
      spr.fillTriangle(x - 7, y - 8, x - 7, y + 8, x + 5, y, c);
      spr.fillRect(x + 6, y - 8, 3, 17, c);
      break;
    case 4:
      spr.setTextColor(c);
      spr.drawString("V+", x, y, 2);
      break;
  }
}

void fitText(const char* src, int font, int maxW, char* out, size_t outSize) {
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

void drawMusic() {
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

  const char* head;
  uint16_t headColor;
  if (!linked) { head = "NOT CONNECTED"; headColor = TFT_ORANGE; }
  else if (playState == 1) { head = "PLAYING"; headColor = TFT_GREEN; }
  else if (playState == 0) { head = "PAUSED"; headColor = TFT_YELLOW; }
  else { head = "MUSIC"; headColor = TFT_CYAN; }

  spr.setTextColor(headColor, TFT_BLACK);
  spr.drawString(head, CX, 36, 2);

  char buf[100];
  if (!linked) {
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString("Pair \"DIY Watch\" in", CX, 72, 2);
    spr.drawString("iPhone Bluetooth settings", CX, 92, 2);
  } else if (showTrack) {
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    int tf = (spr.textWidth(s.title, 4) <= 200) ? 4 : 2;
    fitText(s.title, tf, 200, buf, sizeof(buf));
    spr.drawString(buf, CX, 66, tf);

    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    fitText(s.artist, 2, 200, buf, sizeof(buf));
    spr.drawString(buf, CX, 94, 2);

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
    else snprintf(buf, sizeof(buf), "%d:%02d", e / 60, e % 60);
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

void drawNotifBadge(int y) {
  int n = notifUnread();
  if (n <= 0) return;
  char buf[16];
  snprintf(buf, sizeof(buf), "%d new", n);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_ORANGE, TFT_BLACK);
  spr.drawString(buf, CX, y, 2);
}

// Greedy word wrap into at most maxLines lines; the last line gets "..." if text was cut.
static int wrapText(const char* s, int font, int maxW, int maxLines, char lines[][40]) {
  int n = 0;
  char cur[40] = "";
  size_t cl = 0;
  bool dropped = false;
  const char* p = s;

  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;
    const char* w = p;
    while (*p && *p != ' ') p++;
    size_t wl = p - w;

    char test[40];
    if (cl + 1 + wl < sizeof(test)) {
      if (cl) snprintf(test, sizeof(test), "%s %.*s", cur, (int)wl, w);
      else    snprintf(test, sizeof(test), "%.*s", (int)wl, w);
      if (spr.textWidth(test, font) <= maxW) {
        strlcpy(cur, test, sizeof(cur));
        cl = strlen(cur);
        continue;
      }
    }

    if (cl) {                                   // flush the current line
      strlcpy(lines[n++], cur, 40);
      cur[0] = 0;
      cl = 0;
      if (n >= maxLines) { dropped = true; break; }
    }

    size_t k = (wl < sizeof(cur) - 1) ? wl : sizeof(cur) - 1;   // very long word: clip it
    memcpy(cur, w, k);
    cur[k] = 0;
    while (k > 1 && spr.textWidth(cur, font) > maxW) cur[--k] = 0;
    cl = k;
  }
  if (cl && n < maxLines) strlcpy(lines[n++], cur, 40);

  if (dropped && n > 0) {
    char* last = lines[n - 1];
    size_t k = strlen(last);
    char tmp[48];
    for (;;) {
      snprintf(tmp, sizeof(tmp), "%s...", last);
      if (k == 0 || spr.textWidth(tmp, font) <= maxW) break;
      last[--k] = 0;
    }
    strlcpy(last, tmp, 40);
  }
  return n;
}

void drawNotifications() {
  Notif cur;
  int idx = 0, total = 0;
  bool have = notifSnapshot(cur, idx, total);
  notifMarkAllRead();

  bool linked = bleConnected;
  int ast = ancsStatus;

  spr.fillSprite(TFT_BLACK);
  spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
  spr.setTextDatum(MC_DATUM);

  if (!linked) {
    spr.setTextColor(TFT_ORANGE, TFT_BLACK);
    spr.drawString("NOT CONNECTED", CX, 36, 2);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString("Pair \"DIY Watch\" in", CX, 72, 2);
    spr.drawString("iPhone Bluetooth settings", CX, 92, 2);
  } else if (ast != ANCS_READY) {
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    spr.drawString("NOTIFICATIONS", CX, 36, 2);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    if (ast == ANCS_IDLE) {
      spr.drawString("Connecting...", CX, 90, 2);
    } else {
      spr.drawString("Not available", CX, 72, 2);
      spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
      spr.drawString("Turn on Show Notifications", CX, 96, 2);
      spr.drawString("for DIY Watch in iPhone", CX, 114, 2);
      spr.drawString("Bluetooth settings", CX, 132, 2);
    }
  } else if (!have) {
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    spr.drawString("NOTIFICATIONS", CX, 36, 2);
    spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
    spr.drawString("No notifications", CX, 110, 2);
  } else {
    char buf[64];

    // app name + time received
    if (cur.at) {
      struct tm t;
      localtime_r(&cur.at, &t);
      snprintf(buf, sizeof(buf), "%s  %02d:%02d", cur.app, t.tm_hour, t.tm_min);
    } else {
      snprintf(buf, sizeof(buf), "%s", cur.app);
    }
    char head[64];
    fitText(buf, 2, 190, head, sizeof(head));
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    spr.drawString(cur.loaded ? head : "Loading...", CX, 40, 2);

    if (cur.loaded) {
      spr.setTextColor(TFT_WHITE, TFT_BLACK);
      int tf = (spr.textWidth(cur.title, 4) <= 200) ? 4 : 2;
      fitText(cur.title, tf, 200, buf, sizeof(buf));
      spr.drawString(buf, CX, 70, tf);

      char lines[4][40];
      int n = wrapText(cur.msg, 2, 190, 4, lines);
      spr.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
      for (int i = 0; i < n; i++) spr.drawString(lines[i], CX, 102 + i * 18, 2);
    }

    snprintf(buf, sizeof(buf), "%d / %d", idx + 1, total);
    spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
    spr.drawString(buf, CX, 184, 2);
  }

  spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
  spr.drawString("A: clear  B: next", CX, 204, 2);
  spr.drawString("hold B: exit", CX, 222, 2);

  spr.pushSprite(0, 0);
}

void status(const char* msg) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, CX, CY, 2);
  Serial.println(msg);
}
