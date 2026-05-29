#include "mdns_service.h"
#include "app_config.h"
#include <ESPmDNS.h>
#include <WiFi.h>

void mdnsInit() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (!MDNS.begin(MDNS_HOSTNAME)) {
        Serial.println("[mDNS] start failed");
        return;
    }
    MDNS.addService("http", "tcp", HTTP_PORT);
    MDNS.addServiceTxt("http", "tcp", "path", "/");
    MDNS.addServiceTxt("http", "tcp", "version", FIRMWARE_VERSION);
    Serial.printf("[mDNS] http://%s.local/\n", MDNS_HOSTNAME);
}
