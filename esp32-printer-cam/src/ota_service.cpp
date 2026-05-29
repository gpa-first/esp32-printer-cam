#include "ota_service.h"
#include "app_config.h"
#include "camera_module.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

void otaInit() {
#if OTA_ENABLE
    if (WiFi.status() != WL_CONNECTED) return;

    ArduinoOTA.setHostname(MDNS_HOSTNAME);
    if (strlen(OTA_PASSWORD) > 0) {
        ArduinoOTA.setPassword(OTA_PASSWORD);
    }

    ArduinoOTA.onStart([]() {
        cameraStreamRequestStop();
        Serial.println("[OTA] start");
    });
    ArduinoOTA.onEnd([]() { Serial.println("[OTA] end"); });
    ArduinoOTA.onError([](ota_error_t e) {
        Serial.printf("[OTA] error %u\n", e);
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] ready (host %s)\n", MDNS_HOSTNAME);
#endif
}

void otaLoop() {
#if OTA_ENABLE
    ArduinoOTA.handle();
#endif
}
