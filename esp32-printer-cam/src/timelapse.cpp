#include "timelapse.h"
#include "app_config.h"
#include "camera_module.h"
#include "sd_card.h"
#include <time.h>

static bool g_enabled = false;
static uint32_t g_intervalSec = TIMELAPSE_DEFAULT_INTERVAL_SEC;
static uint32_t g_shotCount = 0;
static uint32_t g_lastShotMs = 0;
static char g_sessionDir[64] = {0};

static String timeStamp() {
    struct tm ti;
    if (!getLocalTime(&ti, 100)) {
        return String(millis());
    }
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &ti);
    return String(buf);
}

void timelapseInit() {
    sdEnsureDir(TIMELAPSE_FOLDER);
}

void timelapseSetEnabled(bool on) {
    if (on && !g_enabled) {
        snprintf(g_sessionDir, sizeof(g_sessionDir), "%s/%s",
                 TIMELAPSE_FOLDER, timeStamp().c_str());
        sdEnsureDir(g_sessionDir);
        g_shotCount = 0;
        g_lastShotMs = 0;
    }
    g_enabled = on;
}

bool timelapseEnabled() { return g_enabled; }

void timelapseSetIntervalSec(uint32_t sec) {
    if (sec < TIMELAPSE_MIN_INTERVAL_SEC) sec = TIMELAPSE_MIN_INTERVAL_SEC;
    if (sec > TIMELAPSE_MAX_INTERVAL_SEC) sec = TIMELAPSE_MAX_INTERVAL_SEC;
    g_intervalSec = sec;
}

uint32_t timelapseIntervalSec() { return g_intervalSec; }
uint32_t timelapseShotCount() { return g_shotCount; }

String timelapseStatusJson() {
    return String("{\"enabled\":") + (g_enabled ? "true" : "false") +
           ",\"interval\":" + g_intervalSec +
           ",\"shots\":" + g_shotCount +
           ",\"sd\":" + (sdReady() ? "true" : "false") +
           ",\"session\":\"" + String(g_sessionDir) + "\"}";
}

void timelapseLoop() {
    if (!g_enabled || !sdReady()) return;
    if (millis() - g_lastShotMs < g_intervalSec * 1000UL) return;

    uint8_t *buf = nullptr;
    size_t len = 0;
    if (!cameraCaptureSnapshot(&buf, &len)) {
        Serial.println("[TL] capture failed");
        return;
    }

    char path[96];
    snprintf(path, sizeof(path), "%s/img_%05u.jpg", g_sessionDir, g_shotCount);
    if (sdSaveJpeg(path, buf, len)) {
        g_shotCount++;
        g_lastShotMs = millis();
        Serial.printf("[TL] saved %s (%u bytes)\n", path, (unsigned)len);
    }
    free(buf);
}
