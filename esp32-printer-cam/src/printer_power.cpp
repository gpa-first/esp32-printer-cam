#include "printer_power.h"
#include "app_config.h"
#include "camera_module.h"
#include "telegram_bot.h"
#include <WiFi.h>

#if PRINTER_POWER_GPIO_ENABLE

static bool g_stableOn = false;
static bool g_lastStableOn = false;
static uint32_t g_edgeSinceMs = 0;
static bool g_pendingOn = false;

static bool readPrinterPowerRaw() {
    int level = digitalRead(PRINTER_POWER_GPIO);
#if PRINTER_POWER_ACTIVE_HIGH
    return level == HIGH;
#else
    return level == LOW;
#endif
}

static void sendPrinterOnTelegram(const char *photoCaption) {
    telegramSendMessage("🔌 <b>Принтер включён</b>\nКамера запущена");

#if PRINTER_BOOT_NOTIFY_PHOTO
    uint8_t *jpg = nullptr;
    size_t len = 0;
    if (cameraCaptureSnapshot(&jpg, &len, true)) {
        String cap = String(photoCaption) + " | " + cameraSnapshotResolutionName() +
                       " Q" + String(cameraSnapshotQuality());
        telegramSendPhoto(jpg, len, cap);
        free(jpg);
    }
#endif
}

void printerPowerInit() {
#if PRINTER_POWER_ACTIVE_HIGH
    pinMode(PRINTER_POWER_GPIO, INPUT_PULLDOWN);
#else
    pinMode(PRINTER_POWER_GPIO, INPUT_PULLUP);
#endif
    g_stableOn = readPrinterPowerRaw();
    g_lastStableOn = g_stableOn;
    g_pendingOn = false;
    Serial.printf("[PWR] GPIO%d sense, state=%s\n",
                  PRINTER_POWER_GPIO, g_stableOn ? "ON" : "OFF");
}

bool printerPowerIsOn() {
    return g_stableOn;
}

void printerPowerLoop() {
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll < PRINTER_POWER_POLL_MS) return;
    lastPoll = millis();

    bool raw = readPrinterPowerRaw();

    if (raw && !g_stableOn) {
        if (!g_pendingOn) {
            g_pendingOn = true;
            g_edgeSinceMs = millis();
        } else if (millis() - g_edgeSinceMs >= PRINTER_POWER_DEBOUNCE_MS) {
            g_stableOn = true;
            g_pendingOn = false;
        }
    } else if (!raw) {
        g_pendingOn = false;
        g_stableOn = false;
    }

    if (g_stableOn && !g_lastStableOn) {
        Serial.println("[PWR] printer ON (GPIO)");
        sendPrinterOnTelegram("Снимок при включении");
    }
    g_lastStableOn = g_stableOn;
}

void printerNotifyBoot() {
#if PRINTER_POWER_ON_BOOT
    if (WiFi.status() != WL_CONNECTED) return;
    Serial.println("[PWR] boot notify (питание с принтером)");
    sendPrinterOnTelegram("Снимок при старте");
#endif
}

#else // камера на одном питании с принтером

void printerPowerInit() {
    Serial.println("[PWR] co-power: уведомление при каждом включении (boot)");
}

bool printerPowerIsOn() {
    return true;
}

void printerPowerLoop() {}

void printerNotifyBoot() {
#if PRINTER_POWER_ON_BOOT
    if (WiFi.status() != WL_CONNECTED) return;
    Serial.println("[PWR] printer+camera power on");

    String msg = "🔌 <b>Принтер включён</b>\n";
    msg += "Камера: " + String(cameraSensorName()) + "\n";
    msg += "Снимки: " + String(cameraSnapshotResolutionName()) +
           " (Q" + String(cameraSnapshotQuality()) + ")\n";
    msg += "IP: " + WiFi.localIP().toString();
    telegramSendMessage(msg);

#if PRINTER_BOOT_NOTIFY_PHOTO
    uint8_t *jpg = nullptr;
    size_t len = 0;
    if (cameraCaptureSnapshot(&jpg, &len, true)) {
        String cap = "Старт печати | " + String(cameraSnapshotResolutionName());
        telegramSendPhoto(jpg, len, cap);
        free(jpg);
    } else {
        telegramSendMessage("⚠️ Не удалось сделать снимок при старте");
    }
#endif
#endif
}

#endif
