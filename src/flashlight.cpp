#include "flashlight.h"

// tft, spr, CX and CY are defined once in display.cpp (declared extern in config.h).

bool flashlightOn = false;

void toggleFlashlight() {
  flashlightOn = !flashlightOn;
}

void drawFlashlight() {
  spr.fillSprite(flashlightOn ? TFT_WHITE : TFT_BLACK);
  spr.setTextDatum(MC_DATUM);

  if (flashlightOn) {
    // small dark hint so the controls can still be found on the white screen
    spr.setTextColor(TFT_DARKGREY, TFT_WHITE);
    spr.drawString("A: off   B: next", CX, 212, 2);
  } else {
    spr.drawCircle(CX, CY, 119, TFT_DARKGREY);
    spr.setTextColor(TFT_CYAN, TFT_BLACK);
    spr.drawString("FLASHLIGHT", CX, 90, 4);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString("A: turn on", CX, 140, 2);
    spr.setTextColor(TFT_DARKGREY, TFT_BLACK);
    spr.drawString("B: next screen", CX, 204, 2);
  }

  spr.pushSprite(0, 0);
}
