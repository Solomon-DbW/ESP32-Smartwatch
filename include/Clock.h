#pragma once
#include <time.h>

void clockBegin();
bool clockIsSynced();
bool clockGetLocalTime(struct tm& result);
