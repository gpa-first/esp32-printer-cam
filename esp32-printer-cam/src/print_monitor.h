#pragma once

#include <Arduino.h>

void monitorInit();
void monitorLoop();
void monitorSetEnabled(bool on);
bool monitorEnabled();
void monitorSetIntervalSec(uint32_t sec, bool persist = true);
uint32_t monitorIntervalSec();
void monitorSetMotionThreshold(float pct, bool persist = true);
float monitorMotionThreshold();
float monitorLastMotionPercent();
String monitorStatusJson();
