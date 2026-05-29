#pragma once

#include <Arduino.h>

// Сравнение двух кадров 96x96 grayscale (%), по мотивам alanesq/CameraWifiMotion
float motionDetectPercent();
bool motionDetectSample(uint8_t *out, size_t n);
