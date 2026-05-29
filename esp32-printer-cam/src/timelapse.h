#pragma once

#include <Arduino.h>

void timelapseInit();
void timelapseLoop();
void timelapseSetEnabled(bool on);
bool timelapseEnabled();
void timelapseSetIntervalSec(uint32_t sec);
uint32_t timelapseIntervalSec();
uint32_t timelapseShotCount();
String timelapseStatusJson();
