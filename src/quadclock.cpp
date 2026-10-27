#include "quadclock.h"

bool clock24h = false;

// ---- layout (all in screen pixels, 240 x 240 round display) -----------------
//
// Each digit is a 7-segment glyph in a DIG_W x DIG_H box tucked against the
// centre cross. With these sizes every corner of every box is at most ~108 px
// from the centre (120, 120), so the whole digit stays inside the visible circle.
static const int DIG_W = 52;
static const int DIG_H = 84;
static const int SEG_T = 9;         // segment thickness
static const int SEG_GAP = 2;       // gap between segments
static const int GAP_X = 8;         // distance from the vertical divider (x = 120)
static const int GAP_Y = 6;         // distance from the horizontal divider (y = 120)

static const uint16_t COL_HOUR = TFT_WHITE;
static const uint16_t COL_MIN  = TFT_CYAN;
static const uint16_t COL_OFF  = 0x2104;    // very dark grey: unlit segments

#define QC_SHOW_UNLIT 1                     // 0 = draw only the lit segments

// segment bits: a=top, b=top-right, c=bottom-right, d=bottom, e=bottom-left, f=top-left, g=middle
static const uint8_t DIGIT_MASK[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

// Draw one digit. digit < 0 draws a blank (all segments unlit).
static void drawDigit(int x, int y, int digit, uint16_t color) {
  const int t = SEG_T, g = SEG_GAP, w = DIG_W, h = DIG_H;
  const int hLen = w - 2 * t - 2 * g;                 // horizontal segment length
  const int vLen = h / 2 - (3 * t) / 2 - 2 * g;       // vertical segment length
  const int midY = y + h / 2 - t / 2;
  const int lowY = y + h / 2 + t / 2 + g;             // top of the lower vertical segments

  uint8_t mask = (digit >= 0 && digit <= 9) ? DIGIT_MASK[digit] : 0;

  struct Seg { int x, y, w, h; };
  const Seg segs[7] = {
    { x + t + g,   y,             hLen, t    },   // a
    { x + w - t,   y + t + g,     t,    vLen },   // b
    { x + w - t,   lowY,          t,    vLen },   // c
    { x + t + g,   y + h - t,     hLen, t    },   // d
    { x,           lowY,          t,    vLen },   // e
    { x,           y + t + g,     t,    vLen },   // f
    { x + t + g,   midY,          hLen, t    },   // g
  };

  for (int i = 0; i < 7; i++) {
    bool on = mask & (1 << i);
#if !QC_SHOW_UNLIT
    if (!on) continue;
#endif
    spr.fillRoundRect(segs[i].x, segs[i].y, segs[i].w, segs[i].h, 3, on ? color : COL_OFF);
  }
}

void drawQuadClock(const struct tm& t) {
  int hour = t.tm_hour;
  bool pm = hour >= 12;
  if (!clock24h) {
    hour %= 12;
    if (hour == 0) hour = 12;
  }

  int hTens = hour / 10;
  int hOnes = hour % 10;
  int mTens = t.tm_min / 10;
  int mOnes = t.tm_min % 10;
  if (!clock24h && hTens == 0) hTens = -1;           // no leading zero in 12-hour mode

  const int leftX  = CX - GAP_X - DIG_W;             // 60
  const int rightX = CX + GAP_X;                     // 128
  const int topY   = CY - GAP_Y - DIG_H;             // 30
  const int botY   = CY + GAP_Y;                     // 126

  spr.fillSprite(TFT_BLACK);

  drawDigit(leftX,  topY, hTens, COL_HOUR);          // top-left
  drawDigit(rightX, topY, hOnes, COL_HOUR);          // top-right
  drawDigit(leftX,  botY, mTens, COL_MIN);           // bottom-left
  drawDigit(rightX, botY, mOnes, COL_MIN);           // bottom-right

  // blinking dot where the four quadrants meet (replaces the colon)
  spr.fillCircle(CX, CY, 4, (t.tm_sec % 2 == 0) ? TFT_RED : COL_OFF);

  spr.setTextDatum(MC_DATUM);

  char buf[16];
  strftime(buf, sizeof(buf), "%a %d", &t);           // e.g. "Tue 27"
  spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
  spr.drawString(buf, CX, 14, 2);

  spr.setTextColor(TFT_GREEN, TFT_BLACK);
  spr.drawString(clock24h ? "24H" : (pm ? "PM" : "AM"), CX, 224, 2);

  spr.pushSprite(0, 0);
}
