#include "sd_card.h"
#include "FS.h"
#include "SD_MMC.h"

static bool g_sdOk = false;

bool sdInit() {
    // ESP32-CAM AI-Thinker: SD_MMC 1-bit
    // AI-Thinker ESP32-CAM: CLK=14, CMD=15, D0=2 (1-bit mode)
    if (!SD_MMC.setPins(14, 15, 2)) {
        Serial.println("[SD] setPins failed");
        return false;
    }
    if (!SD_MMC.begin("/sdcard", true, false, 40000)) {
        Serial.println("[SD] mount failed — таймлапс на SD недоступен");
        g_sdOk = false;
        return false;
    }
    g_sdOk = true;
    Serial.printf("[SD] OK, %llu MB free / %llu MB total\n",
                  (unsigned long long)(SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024 * 1024),
                  (unsigned long long)SD_MMC.totalBytes() / (1024 * 1024));
    return true;
}

bool sdReady() { return g_sdOk; }

bool sdEnsureDir(const char *path) {
    if (!g_sdOk) return false;
    if (SD_MMC.exists(path)) return true;
    return SD_MMC.mkdir(path);
}

bool sdSaveJpeg(const char *path, const uint8_t *data, size_t len) {
    if (!g_sdOk || !data || !len) return false;
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) return false;
    size_t w = f.write(data, len);
    f.close();
    return w == len;
}

uint64_t sdFreeBytes() {
    if (!g_sdOk) return 0;
    return SD_MMC.totalBytes() - SD_MMC.usedBytes();
}

uint64_t sdTotalBytes() {
    if (!g_sdOk) return 0;
    return SD_MMC.totalBytes();
}
