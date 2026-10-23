#pragma once
#include <Arduino.h>

extern bool swRunning;
extern uint32_t swStartMs;
extern uint32_t swBaseMs;

uint32_t stopwatchElapsed();
void stopwatchToggle();
void stopwatchReset();
