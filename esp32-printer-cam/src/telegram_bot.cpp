#include "telegram_bot.h"
#include "app_config.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "Create include/secrets.h from secrets.example.h"
#define TELEGRAM_BOT_TOKEN ""
#define TELEGRAM_CHAT_ID ""
#endif

static TelegramCommandHandler g_handler;
static int64_t g_lastUpdateId = 0;
static WiFiClientSecure g_tls;

static String apiUrl(const String &method) {
    return "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/" + method;
}

bool telegramInit() {
    if (strlen(TELEGRAM_BOT_TOKEN) < 10) {
        Serial.println("[TG] token not configured");
        return false;
    }
    g_tls.setInsecure();
    return true;
}

bool telegramSendMessage(const String &text) {
    if (strlen(TELEGRAM_BOT_TOKEN) < 10) return false;

    HTTPClient http;
    http.begin(g_tls, apiUrl("sendMessage"));
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["chat_id"] = TELEGRAM_CHAT_ID;
    doc["text"] = text;
    doc["parse_mode"] = "HTML";

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    http.end();
    return code == 200;
}

bool telegramSendPhoto(const uint8_t *jpg, size_t len, const String &caption) {
    if (!jpg || !len || strlen(TELEGRAM_BOT_TOKEN) < 10) return false;

    String boundary = "----Esp32CamBoundary7MA4YWxk";
    String head = "--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
    head += TELEGRAM_CHAT_ID;
    head += "\r\n--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"caption\"\r\n\r\n";
    head += caption;
    head += "\r\n--" + boundary + "\r\n";
    head += "Content-Disposition: form-data; name=\"photo\"; filename=\"shot.jpg\"\r\n";
    head += "Content-Type: image/jpeg\r\n\r\n";

    String tail = "\r\n--" + boundary + "--\r\n";
    size_t totalLen = head.length() + len + tail.length();

    WiFiClient *stream = g_tls.connect("api.telegram.org", 443);
    if (!stream) return false;

    stream->printf("POST /bot%s/sendPhoto HTTP/1.1\r\n", TELEGRAM_BOT_TOKEN);
    stream->print("Host: api.telegram.org\r\n");
    stream->print("Content-Type: multipart/form-data; boundary=");
    stream->println(boundary);
    stream->printf("Content-Length: %u\r\n\r\n", (unsigned)totalLen);
    stream->print(head);
    stream->write(jpg, len);
    stream->print(tail);

    uint32_t timeout = millis() + 30000;
    while (stream->connected() && !stream->available() && millis() < timeout) {
        delay(10);
    }

  String response;
    while (stream->available()) {
        response += (char)stream->read();
    }
    delete stream;
    return response.indexOf("\"ok\":true") >= 0;
}

static void processMessage(const String &text) {
    String t = text;
    t.trim();
    if (!t.startsWith("/")) return;

    int sp = t.indexOf(' ');
    String cmd = (sp < 0) ? t : t.substring(0, sp);
    String args = (sp < 0) ? "" : t.substring(sp + 1);
    cmd.toLowerCase();
    if (g_handler) g_handler(cmd, args);
}

void telegramLoop() {
    if (strlen(TELEGRAM_BOT_TOKEN) < 10) return;

    static uint32_t lastPoll = 0;
    if (millis() - lastPoll < TELEGRAM_POLL_INTERVAL_MS) return;
    lastPoll = millis();

    HTTPClient http;
    String url = apiUrl("getUpdates") + "?timeout=0&offset=" + String(g_lastUpdateId + 1);
    http.begin(g_tls, url);
    int code = http.GET();
    if (code != 200) {
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload)) return;
    if (!doc["ok"].as<bool>()) return;

    for (JsonObject upd : doc["result"].as<JsonArray>()) {
        g_lastUpdateId = upd["update_id"].as<int64_t>();
        const char *txt = upd["message"]["text"];
        if (txt) processMessage(txt);
    }
}

void telegramSetCommandHandler(TelegramCommandHandler handler) {
    g_handler = handler;
}
