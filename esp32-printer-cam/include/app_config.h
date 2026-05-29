#pragma once

// Плата (см. include/camera_pins.h)
#define CAMERA_MODEL_AI_THINKER
// #define CAMERA_MODEL_WROVER_KIT
// #define CAMERA_MODEL_ESP_EYE

#define CAM_NAME "ESP32 Printer Cam"
#define FIRMWARE_VERSION "1.2.0"

// mDNS — http://esp32-printer-cam.local/ ([esp32cam-rtsp](https://github.com/rzeldent/esp32cam-rtsp))
#define MDNS_HOSTNAME              "esp32-printer-cam"

// --- Камера / поток ---
#define STREAM_FRAMESIZE   FRAMESIZE_SVGA
#define STREAM_QUALITY     12
#define STREAM_MIN_FRAME_MS        50   // лимит FPS (~20 fps max), как min_frame_time
#define STREAM_AUTOLAMP            1    // подсветка на время MJPEG-стрима
#define STREAM_LAMP_PERCENT        30   // яркость при стриме (0–100)

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
#define TIMELAPSE_MAX_FILES_SESSION    500  // ротация: удалять старые кадры в сессии

// --- Мониторинг печати ---
#define MONITOR_DEFAULT_INTERVAL_SEC   60
#define MONITOR_STALL_MINUTES          15
#define MONITOR_MOTION_THRESHOLD       8.0f
#define MONITOR_SAMPLE_SIZE            32
#define MONITOR_PHOTO_ON_MOTION_ONLY   1    // фото в TG только при движении (экономия)
#define MONITOR_FORCE_PHOTO_EVERY_N    10   // иначе каждые N проверок — контрольный кадр
#define MONITOR_TELEGRAM_COOLDOWN_MS   30000

// --- PIR (AM312), опционально — JJFourie / mtnbkr88 ---
#define PIR_SENSOR_ENABLE              0
#define PIR_GPIO                       13   // не GPIO12 — он влияет на загрузку с флеша
#define PIR_COOLDOWN_MS                60000

// --- Telegram ---
#define TELEGRAM_POLL_INTERVAL_MS      2000
#define TELEGRAM_MAX_MESSAGE_LEN         4096

// --- Веб-сервер ---
#define HTTP_PORT                      80
#define WEB_AUTH_ENABLE                0    // 1 + WEB_AUTH_* в secrets.h

// --- OTA ([topic esp32cam](https://github.com/topics/esp32cam)) ---
#define OTA_ENABLE                     1

// Стабильность питания (как в esp32cam-rtsp)
#define BROWNOUT_DISABLE               1

// NTP (для имён файлов таймлапса)
#define NTP_SERVER                     "pool.ntp.org"
#define GMT_OFFSET_SEC                 3 * 3600
#define DAYLIGHT_OFFSET_SEC            0

// --- Подсветка / вспышка (LAMP_PIN в camera_pins.h) ---
#define FLASH_PWM_CHANNEL              7
#define FLASH_PWM_FREQ_HZ              50000
#define FLASH_PWM_RES_BITS             9
#define FLASH_LAMP_PERCENT             100  // яркость для снимков (0–100)
#define FLASH_WARMUP_MS                80    // выдержка после включения
#define FLASH_DARK_AVG_THRESHOLD       48    // средняя яркость 0..255 — ниже → вспышка
#define FLASH_AUTO_ENABLE              true

// --- Датчик включения принтера по GPIO (только если GPIO_ENABLE=1) ---
#define PRINTER_POWER_GPIO             33
#define PRINTER_POWER_ACTIVE_HIGH      1     // 1 = HIGH когда принтер вкл
#define PRINTER_POWER_DEBOUNCE_MS      2000
#define PRINTER_POWER_POLL_MS          100
