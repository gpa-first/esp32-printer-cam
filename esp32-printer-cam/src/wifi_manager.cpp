#include "wifi_manager.h"
#include <WiFi.h>

#define WIFI_RECONNECT_INTERVAL_MS 15000

void wifiManagerLoop() {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck < WIFI_RECONNECT_INTERVAL_MS) return;
    lastCheck = millis();

    if (WiFi.status() == WL_CONNECTED) return;

    Serial.println("[WiFi] reconnecting...");
    WiFi.reconnect();
}
