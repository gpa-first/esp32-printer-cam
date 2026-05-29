#pragma once

#include "esp_camera.h"
#include <Arduino.h>

struct CameraInfo {
    bool ok = false;
    sensor_id_t sensorId{};
    const char *name = "unknown";
    framesize_t maxFrameSize = FRAMESIZE_VGA;
};

bool cameraInitAuto(CameraInfo &info);
framesize_t cameraSnapshotSize();
int cameraSnapshotQuality();
const char *cameraSnapshotResolutionName();
bool cameraCaptureSnapshot(uint8_t **buf, size_t *len, bool autoFlash = true);
bool cameraCaptureJpeg(framesize_t size, int quality, uint8_t **buf, size_t *len,
                       bool autoFlash = true);
bool cameraIsDarkScene();
void cameraFlashInit();
void cameraFlashSet(uint8_t brightness);
void cameraReleaseFrame(camera_fb_t *fb);
const char *cameraSensorName();
