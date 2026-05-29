#pragma once

#include <Arduino.h>

void monitorInit();
void monitorLoop();
void monitorSetEnabled(bool on);
bool monitorEnabled();
void monitorSetIntervalSec(uint32_t sec);
uint32_t monitorIntervalSec();
float monitorLastMotionPercent();
String monitorStatusJson();
