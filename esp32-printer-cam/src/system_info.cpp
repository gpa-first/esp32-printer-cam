#include "system_info.h"
#include <WiFi.h>

extern "C" uint8_t temprature_sens_read();

static String jsonEscape(const String &s) {
    String out;
    out.reserve(s.length() + 8);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    return out;
}

float systemChipTempC() {
    return (temprature_sens_read() - 32) / 1.8f;
}

String systemInfoJson() {
    String j = "{";
    j += "\"uptime_s\":" + String(millis() / 1000);
    j += ",\"heap_free\":" + String(ESP.getFreeHeap());
    j += ",\"heap_size\":" + String(ESP.getHeapSize());
    j += ",\"chip_temp_c\":" + String(systemChipTempC(), 1);
    j += ",\"rssi\":" + String(WiFi.RSSI());
    if (WiFi.status() == WL_CONNECTED) {
        j += ",\"wifi_ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
    }
    j += "}";
    return j;
}
