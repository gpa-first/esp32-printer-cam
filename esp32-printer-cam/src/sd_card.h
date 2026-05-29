#pragma once

#include <Arduino.h>

bool sdInit();
bool sdReady();
bool sdSaveJpeg(const char *path, const uint8_t *data, size_t len);
bool sdEnsureDir(const char *path);
uint64_t sdFreeBytes();
uint64_t sdTotalBytes();
