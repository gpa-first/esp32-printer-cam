#include "sd_card.h"
#include "FS.h"
#include "SD_MMC.h"
#include <cstring>

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

void sdPruneOldestJpegs(const char *dir, uint32_t keepMax) {
    if (!g_sdOk || !dir || keepMax == 0) return;

    File root = SD_MMC.open(dir);
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return;
    }

    struct Entry {
        char name[48];
        uint32_t num;
    };
    Entry *entries = (Entry *)malloc(sizeof(Entry) * keepMax * 2);
    if (!entries) {
        root.close();
        return;
    }
    uint32_t count = 0;

    File f = root.openNextFile();
    while (f) {
        if (!f.isDirectory()) {
            const char *n = f.name();
            const char *base = strrchr(n, '/');
            base = base ? base + 1 : n;
            if (strstr(base, ".jpg") || strstr(base, ".JPG")) {
                uint32_t num = 0;
                if (sscanf(base, "img_%u.jpg", &num) == 1 ||
                    sscanf(base, "img_%u.JPG", &num) == 1) {
                    if (count < keepMax * 2) {
                        strncpy(entries[count].name, base, sizeof(entries[count].name) - 1);
                        entries[count].num = num;
                        count++;
                    }
                }
            }
        }
        f.close();
        f = root.openNextFile();
    }
    root.close();

    if (count <= keepMax) {
        free(entries);
        return;
    }

    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            if (entries[j].num < entries[i].num) {
                Entry t = entries[i];
                entries[i] = entries[j];
                entries[j] = t;
            }
        }
    }

    uint32_t toDelete = count - keepMax;
    for (uint32_t i = 0; i < toDelete; i++) {
        char path[96];
        snprintf(path, sizeof(path), "%s/%s", dir, entries[i].name);
        if (SD_MMC.remove(path)) {
            Serial.printf("[SD] pruned %s\n", path);
        }
    }
    free(entries);
}
