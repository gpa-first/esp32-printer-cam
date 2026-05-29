#include "app_config.h"
#include "camera_module.h"
#include "print_monitor.h"
#include "printer_power.h"
#include "sd_card.h"
#include "telegram_bot.h"
#include "timelapse.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "settings_store.h"
#include "pir_sensor.h"
#include "system_info.h"
#include "mdns_service.h"
#include "ota_service.h"

#include <WiFi.h>
#include <ESP.h>
#include <esp_wifi.h>
#if BROWNOUT_DISABLE
#include "soc/rtc_cntl_reg.h"
#endif

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "Create include/secrets.h from secrets.example.h"
#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASSWORD"
#endif

static CameraInfo g_camInfo;

static void setupNtp() {
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm ti;
    for (int i = 0; i < 20; i++) {
        if (getLocalTime(&ti, 500)) {
            Serial.println("[NTP] time synced");
            return;
        }
        delay(200);
    }
    Serial.println("[NTP] sync timeout (имена файлов — по millis)");
}

static void connectWifi() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.printf("[WiFi] connecting to %s", WIFI_SSID);
    uint8_t dots = 0;
    while (WiFi.status() != WL_CONNECTED && dots < 60) {
        delay(500);
        Serial.print('.');
        dots++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] OK %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[WiFi] failed — перезагрузите модуль");
    }
}

static void handleTelegramCommand(const String &cmd, const String &args) {
    if (cmd == "/start" || cmd == "/help") {
        telegramSendMessage(
            "<b>ESP32 Printer Cam</b>\n"
            "/photo — снимок\n"
            "/status — состояние\n"
            "/monitor_on [сек] — мониторинг печати\n"
            "/monitor_off\n"
            "/timelapse_on [сек] — таймлапс на SD\n"
            "/timelapse_off\n"
            "/reboot — перезагрузка (веб: /reboot)");
        return;
    }

    if (cmd == "/reboot") {
        telegramSendMessage("🔄 Перезагрузка...");
        settingsSave();
        delay(300);
        ESP.restart();
        return;
    }

    if (cmd == "/photo") {
        uint8_t *jpg = nullptr;
        size_t len = 0;
        if (cameraCaptureSnapshot(&jpg, &len)) {
            telegramSendPhoto(jpg, len, String("📷 ") + cameraSnapshotResolutionName());
            free(jpg);
        } else {
            telegramSendMessage("Ошибка захвата кадра");
        }
        return;
    }

    if (cmd == "/status") {
        String msg = "📡 <b>Статус</b>\n";
        msg += "Камера: " + String(cameraSensorName()) + "\n";
        msg += "IP: " + WiFi.localIP().toString() + "\n";
        msg += "mDNS: http://" + String(MDNS_HOSTNAME) + ".local\n";
        msg += "Heap: " + String(ESP.getFreeHeap()) + " B\n";
        msg += "SD: " + String(sdReady() ? "OK" : "нет") + "\n";
        msg += "Таймлапс: " + String(timelapseEnabled() ? "вкл" : "выкл") +
               " (" + String(timelapseShotCount()) + " кадров)\n";
        msg += "Мониторинг: " + String(monitorEnabled() ? "вкл" : "выкл") +
               ", движение " + String(monitorLastMotionPercent(), 1) + "%\n";
        msg += "Снимки: " + String(cameraSnapshotResolutionName()) +
               " Q" + String(cameraSnapshotQuality()) + "\n";
        msg += "Темп. чипа: " + String(systemChipTempC(), 1) + " °C";
        telegramSendMessage(msg);
        return;
    }

    if (cmd == "/monitor_on") {
        uint32_t sec = args.toInt();
        if (sec > 0) monitorSetIntervalSec(sec);
        monitorSetEnabled(true);
        telegramSendMessage("✅ Мониторинг печати включён (" +
                            String(monitorIntervalSec()) + " сек)");
        return;
    }
    if (cmd == "/monitor_off") {
        monitorSetEnabled(false);
        telegramSendMessage("⏹ Мониторинг выключен");
        return;
    }

    if (cmd == "/timelapse_on") {
        if (!sdReady()) {
            telegramSendMessage("❌ SD-карта не найдена");
            return;
        }
        uint32_t sec = args.toInt();
        if (sec > 0) timelapseSetIntervalSec(sec);
        timelapseSetEnabled(true);
        telegramSendMessage("✅ Таймлапс на SD (" + String(timelapseIntervalSec()) + " сек)");
        return;
    }
    if (cmd == "/timelapse_off") {
        timelapseSetEnabled(false);
        telegramSendMessage("⏹ Таймлапс остановлен. Кадров: " + String(timelapseShotCount()));
        return;
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.printf("=== %s v%s ===\n", CAM_NAME, FIRMWARE_VERSION);

#if BROWNOUT_DISABLE
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

    if (!cameraInitAuto(g_camInfo)) {
        Serial.println("FATAL: camera init failed. Проверьте шлейф и питание 5V/3.3V");
        while (true) delay(1000);
    }

    settingsLoad();

    sdInit();
    timelapseInit();
    monitorInit();
    printerPowerInit();
    pirInit();

    connectWifi();
    setupNtp();
    mdnsInit();
    otaInit();

    telegramInit();
    telegramSetCommandHandler(handleTelegramCommand);
    printerNotifyBoot();

    webInit();
}

void loop() {
    webLoop();
    telegramLoop();
    timelapseLoop();
    monitorLoop();
    printerPowerLoop();
    wifiManagerLoop();
    otaLoop();
    pirLoop();
    delay(2);
}
