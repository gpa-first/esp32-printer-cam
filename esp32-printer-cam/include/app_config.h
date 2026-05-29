#pragma once

// --- Камера / поток ---
#define STREAM_FRAMESIZE   FRAMESIZE_SVGA   // 800x600 — баланс для стрима
#define STREAM_QUALITY     12

// Снимки: макс. разрешение сенсора + лучший JPEG (0 = лучшее, 63 = хуже)
#define SNAPSHOT_QUALITY           4
#define SNAPSHOT_WARMUP_FRAMES     2    // кадры на автоэкспозицию перед съёмкой

// Камера включается вместе с принтером → уведомление при старте прошивки
#define PRINTER_POWER_ON_BOOT      1
#define PRINTER_BOOT_NOTIFY_PHOTO  1

// Опционально: отдельный GPIO, если ESP всегда под питанием (0 = выкл)
#define PRINTER_POWER_GPIO_ENABLE  0

// --- Таймлапс (SD-карта) ---
#define TIMELAPSE_DEFAULT_INTERVAL_SEC 30
#define TIMELAPSE_MIN_INTERVAL_SEC     5
#define TIMELAPSE_MAX_INTERVAL_SEC     3600
#define TIMELAPSE_FOLDER               "/timelapse"

// --- Мониторинг печати ---
#define MONITOR_DEFAULT_INTERVAL_SEC   60
#define MONITOR_STALL_MINUTES          15   // нет изменений N мин → «возможно зависла»
#define MONITOR_MOTION_THRESHOLD       8.0f // % изменения пикселей между кадрами
#define MONITOR_SAMPLE_SIZE            32   // размер для сравнения кадров

// --- Telegram ---
#define TELEGRAM_POLL_INTERVAL_MS      2000
#define TELEGRAM_MAX_MESSAGE_LEN         4096

// --- Веб-сервер ---
#define HTTP_PORT                      80

// NTP (для имён файлов таймлапса)
#define NTP_SERVER                     "pool.ntp.org"
#define GMT_OFFSET_SEC                 3 * 3600
#define DAYLIGHT_OFFSET_SEC            0

// --- Вспышка ESP32-CAM (GPIO 4 на AI-Thinker) ---
#define FLASH_LED_GPIO                 4
#define FLASH_PWM_CHANNEL              7
#define FLASH_PWM_FREQ_HZ              5000
#define FLASH_PWM_RES_BITS             8
#define FLASH_BRIGHTNESS               255   // 0..255
#define FLASH_WARMUP_MS                80    // выдержка после включения
#define FLASH_DARK_AVG_THRESHOLD       48    // средняя яркость 0..255 — ниже → вспышка
#define FLASH_AUTO_ENABLE              true

// --- Датчик включения принтера по GPIO (только если GPIO_ENABLE=1) ---
#define PRINTER_POWER_GPIO             33
#define PRINTER_POWER_ACTIVE_HIGH      1     // 1 = HIGH когда принтер вкл
#define PRINTER_POWER_DEBOUNCE_MS      2000
#define PRINTER_POWER_POLL_MS          100
