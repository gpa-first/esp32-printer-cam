#pragma once

// Скопируйте в secrets.h и заполните своими данными:
// cp include/secrets.example.h include/secrets.h

#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// @BotFather -> токен бота
#define TELEGRAM_BOT_TOKEN "123456789:ABCdefGHIjklMNOpqrsTUVwxyz"
// ID чата: напишите боту /start, откройте https://api.telegram.org/bot<TOKEN>/getUpdates
#define TELEGRAM_CHAT_ID   "123456789"

// Опционально: задайте WEB_AUTH_ENABLE 1 в app_config.h и пароль здесь
#define WEB_AUTH_USER      "admin"
#define WEB_AUTH_PASSWORD  "change_me"

// OTA: pio run -t upload --upload-port esp32-printer-cam.local
#define OTA_PASSWORD       "change_me_ota"
