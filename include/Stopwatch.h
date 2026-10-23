#pragma once
#include <Arduino.h>

extern bool swRunning;
void stopwatchBegin();
uint32_t stopwatchElapsed();
void stopwatchToggle();
void stopwatchReset();
