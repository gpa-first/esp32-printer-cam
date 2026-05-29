#include "motion_detect.h"
#include "app_config.h"
#include <esp_camera.h>

bool motionDetectSample(uint8_t *out, size_t n) {
    if (!out || n == 0) return false;

    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;

    framesize_t prevSize = s->status.framesize;

    s->set_framesize(s, FRAMESIZE_96X96);
    s->set_pixformat(s, PIXFORMAT_GRAYSCALE);

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb || !fb->buf || fb->len == 0) {
        if (fb) esp_camera_fb_return(fb);
        s->set_pixformat(s, PIXFORMAT_JPEG);
        s->set_framesize(s, prevSize);
        return false;
    }

    size_t step = max((size_t)1, fb->len / n);
    for (size_t i = 0; i < n; i++) {
        out[i] = fb->buf[i * step];
    }
    esp_camera_fb_return(fb);

    s->set_pixformat(s, PIXFORMAT_JPEG);
    s->set_framesize(s, prevSize);
    return true;
}

float motionDetectPercent() {
    static uint8_t prev[MONITOR_SAMPLE_SIZE];
    static bool havePrev = false;

    uint8_t cur[MONITOR_SAMPLE_SIZE];
    if (!motionDetectSample(cur, MONITOR_SAMPLE_SIZE)) return 0;

    if (!havePrev) {
        memcpy(prev, cur, MONITOR_SAMPLE_SIZE);
        havePrev = true;
        return 0;
    }

    uint32_t diff = 0;
    for (size_t i = 0; i < MONITOR_SAMPLE_SIZE; i++) {
        int d = (int)cur[i] - (int)prev[i];
        if (d < 0) d = -d;
        diff += (uint32_t)d;
    }
    memcpy(prev, cur, MONITOR_SAMPLE_SIZE);

    return (100.0f * diff) / (255.0f * MONITOR_SAMPLE_SIZE);
}
