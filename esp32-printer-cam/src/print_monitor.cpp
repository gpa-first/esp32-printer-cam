#include "print_monitor.h"
#include "app_config.h"
#include "camera_module.h"
#include "settings_store.h"
#include "telegram_bot.h"
#include <esp_camera.h>

static bool g_enabled = false;
static uint32_t g_intervalSec = MONITOR_DEFAULT_INTERVAL_SEC;
static uint32_t g_lastCheckMs = 0;
static uint32_t g_lastMotionMs = 0;
static uint32_t g_lastTelegramMs = 0;
static uint32_t g_checkCounter = 0;
static float g_motionThreshold = MONITOR_MOTION_THRESHOLD;
static float g_lastMotion = 0;
static bool g_stallNotified = false;

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

static bool shouldSendPhoto(bool motionDetected, bool forceStall) {
    if (forceStall) return true;
#if MONITOR_PHOTO_ON_MOTION_ONLY
    if (motionDetected) return true;
    g_checkCounter++;
    if (MONITOR_FORCE_PHOTO_EVERY_N > 0 &&
        (g_checkCounter % MONITOR_FORCE_PHOTO_EVERY_N) == 0) {
        return true;
    }
    return false;
#else
    (void)motionDetected;
    return true;
#endif
}

static void sendMonitorPhoto(const String &caption) {
    if (millis() - g_lastTelegramMs < MONITOR_TELEGRAM_COOLDOWN_MS) return;

    uint8_t *jpg = nullptr;
    size_t len = 0;
    if (!cameraCaptureSnapshot(&jpg, &len, true)) return;

    telegramSendPhoto(jpg, len, caption);
    free(jpg);
    g_lastTelegramMs = millis();
}

void monitorInit() {
    g_lastMotionMs = millis();
    g_motionThreshold = MONITOR_MOTION_THRESHOLD;
}

void monitorSetEnabled(bool on) {
    g_enabled = on;
    g_stallNotified = false;
    g_lastCheckMs = 0;
    g_checkCounter = 0;
    if (on) g_lastMotionMs = millis();
}

bool monitorEnabled() { return g_enabled; }

void monitorSetIntervalSec(uint32_t sec, bool persist) {
    if (sec < 10) sec = 10;
    if (sec > 3600) sec = 3600;
    g_intervalSec = sec;
    if (persist) settingsSave();
}

uint32_t monitorIntervalSec() { return g_intervalSec; }

void monitorSetMotionThreshold(float pct, bool persist) {
    if (pct < 1.0f) pct = 1.0f;
    if (pct > 50.0f) pct = 50.0f;
    g_motionThreshold = pct;
    if (persist) settingsSave();
}

float monitorMotionThreshold() { return g_motionThreshold; }
float monitorLastMotionPercent() { return g_lastMotion; }

String monitorStatusJson() {
    return String("{\"enabled\":") + (g_enabled ? "true" : "false") +
           ",\"interval\":" + g_intervalSec +
           ",\"motion\":" + String(g_lastMotion, 2) +
           ",\"threshold\":" + String(g_motionThreshold, 2) +
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

    bool motion = g_lastMotion >= g_motionThreshold;
    if (motion) {
        g_lastMotionMs = millis();
        g_stallNotified = false;
    }

    uint32_t stallMs = MONITOR_STALL_MINUTES * 60UL * 1000UL;
    if (!g_stallNotified && (millis() - g_lastMotionMs) > stallMs) {
        g_stallNotified = true;
        telegramSendMessage(
            "⚠️ <b>Мониторинг печати</b>\nНет движения " +
            String(MONITOR_STALL_MINUTES) + " мин.");
        sendMonitorPhoto("Снимок: возможное зависание");
        return;
    }

    if (shouldSendPhoto(motion, false)) {
        String cap = "🖨 Печать | движение " + String(g_lastMotion, 1) + "%";
        sendMonitorPhoto(cap);
    }
}
