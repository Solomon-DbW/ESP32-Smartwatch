#pragma once

#include "config.h"
#include "stopwatch.h"
#include "music_remote.h"
#include "app_state.h"

void drawHand(float angleDeg, int length, int width, uint16_t color);
void drawFace();
void drawClock(const struct tm& t);
void drawDigitalClock(const struct tm& t);
void drawStopwatch();
void drawMusicIcon(int idx, int x, int y, bool selected, bool flashing, int playState);
void fitText(const char* src, int font, int maxW, char* out, size_t outSize);
void drawMusic();
void status(const char* msg);
