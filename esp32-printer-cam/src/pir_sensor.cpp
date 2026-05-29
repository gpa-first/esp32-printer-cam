#include "pir_sensor.h"
#include "app_config.h"
#include "camera_module.h"
#include "telegram_bot.h"

#if PIR_SENSOR_ENABLE

static bool g_lastHigh = true;
static uint32_t g_lastTriggerMs = 0;

void pirInit() {
    pinMode(PIR_GPIO, INPUT_PULLUP);
    g_lastHigh = digitalRead(PIR_GPIO) == HIGH;
    Serial.printf("[PIR] GPIO%d active LOW\n", PIR_GPIO);
}

void pirLoop() {
    bool high = digitalRead(PIR_GPIO) == HIGH;
    if (high == g_lastHigh) return;

    g_lastHigh = high;
    if (high) return;

    if (millis() - g_lastTriggerMs < PIR_COOLDOWN_MS) return;
    g_lastTriggerMs = millis();

    Serial.println("[PIR] motion detected");
    telegramSendMessage("👁 <b>PIR: движение</b>");

    uint8_t *jpg = nullptr;
    size_t len = 0;
    if (cameraCaptureSnapshot(&jpg, &len, true)) {
        telegramSendPhoto(jpg, len, "PIR снимок");
        free(jpg);
    }
}

#else

void pirInit() {}
void pirLoop() {}

#endif
