#include "print_monitor.h"
#include "app_config.h"
#include "camera_module.h"
#include "telegram_bot.h"
#include <esp_camera.h>

static bool g_enabled = false;
static uint32_t g_intervalSec = MONITOR_DEFAULT_INTERVAL_SEC;
static uint32_t g_lastCheckMs = 0;
static uint32_t g_lastMotionMs = 0;
static float g_lastMotion = 0;
static bool g_stallNotified = false;

// Сравнение двух JPEG-кадров 96x96 по выборке байт (без смены pixformat)
static float jpegMotionPercent(const uint8_t *a, size_t lenA, const uint8_t *b, size_t lenB) {
    size_t n = min(min(lenA, lenB), (size_t)MONITOR_SAMPLE_SIZE * 64);
    if (n < MONITOR_SAMPLE_SIZE) return 100.0f;

    size_t step = n / MONITOR_SAMPLE_SIZE;
    uint32_t diff = 0;
    for (size_t i = 0; i < MONITOR_SAMPLE_SIZE; i++) {
        int d = (int)a[i * step] - (int)b[i * step];
        if (d < 0) d = -d;
        diff += (uint32_t)d;
    }
    return (100.0f * diff) / (255.0f * MONITOR_SAMPLE_SIZE);
}

static bool captureSmallJpeg(uint8_t **buf, size_t *len) {
    return cameraCaptureJpeg(FRAMESIZE_96X96, 20, buf, len, false);
}

void monitorInit() {
    g_lastMotionMs = millis();
}

void monitorSetEnabled(bool on) {
    g_enabled = on;
    g_stallNotified = false;
    g_lastCheckMs = 0;
    if (on) g_lastMotionMs = millis();
}

bool monitorEnabled() { return g_enabled; }

void monitorSetIntervalSec(uint32_t sec) {
    if (sec < 10) sec = 10;
    if (sec > 3600) sec = 3600;
    g_intervalSec = sec;
}

uint32_t monitorIntervalSec() { return g_intervalSec; }
float monitorLastMotionPercent() { return g_lastMotion; }

String monitorStatusJson() {
    return String("{\"enabled\":") + (g_enabled ? "true" : "false") +
           ",\"interval\":" + g_intervalSec +
           ",\"motion\":" + String(g_lastMotion, 2) +
           ",\"stall_min\":" + MONITOR_STALL_MINUTES + "}";
}

void monitorLoop() {
    if (!g_enabled) return;
    if (millis() - g_lastCheckMs < g_intervalSec * 1000UL) return;
    g_lastCheckMs = millis();

    static uint8_t *prevBuf = nullptr;
    static size_t prevLen = 0;
    static bool havePrev = false;

    uint8_t *curBuf = nullptr;
    size_t curLen = 0;
    if (!captureSmallJpeg(&curBuf, &curLen)) return;

    if (!havePrev) {
        free(prevBuf);
        prevBuf = curBuf;
        prevLen = curLen;
        havePrev = true;
        return;
    }

    g_lastMotion = jpegMotionPercent(prevBuf, prevLen, curBuf, curLen);
    free(prevBuf);
    prevBuf = curBuf;
    prevLen = curLen;

    if (g_lastMotion >= MONITOR_MOTION_THRESHOLD) {
        g_lastMotionMs = millis();
        g_stallNotified = false;
    }

    uint32_t stallMs = MONITOR_STALL_MINUTES * 60UL * 1000UL;
    if (!g_stallNotified && (millis() - g_lastMotionMs) > stallMs) {
        g_stallNotified = true;
        telegramSendMessage(
            "⚠️ <b>Мониторинг печати</b>\nНет заметного движения уже " +
            String(MONITOR_STALL_MINUTES) + " мин.\nПроверьте принтер.");
        uint8_t *jpg = nullptr;
        size_t len = 0;
        if (cameraCaptureSnapshot(&jpg, &len)) {
            telegramSendPhoto(jpg, len, "Снимок при предупреждении о зависании");
            free(jpg);
        }
    }

    uint8_t *jpg = nullptr;
    size_t len = 0;
    if (cameraCaptureSnapshot(&jpg, &len)) {
        String cap = "🖨 Печать | движение " + String(g_lastMotion, 1) + "%";
        telegramSendPhoto(jpg, len, cap);
        free(jpg);
    }
}
