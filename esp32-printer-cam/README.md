# ESP32-CAM — веб-камера для 3D-принтера

Прошивка для **AI-Thinker ESP32-CAM**: MJPEG-стрим в браузере, **автоопределение** сенсора (OV2640, OV3660, OV5640 и др. через `esp32-camera`), **таймлапс** на microSD, **мониторинг печати** с уведомлениями и снимками в **Telegram**.

## Возможности

| Функция | Описание |
|--------|----------|
| Автокамера | Драйвер `esp32-camera` сам определяет сенсор по PID, без жёсткой привязки к OV2640 |
| Веб-интерфейс | `http://<IP>/` — поток, управление таймлапсом и мониторингом |
| Таймлапс | JPEG в `/timelapse/YYYYMMDD_HHMMSS/` на SD (нужна карта) |
| Мониторинг | Периодические фото в Telegram + предупреждение, если нет движения N минут |
| Включение принтера | Telegram при каждом старте (камера на одном питании с принтером) |
| Качество снимков | Максимальное разрешение сенсора + JPEG Q4 |
| Вспышка | Автовключение LED GPIO4 при съёмке в темноте |
| Telegram | Команды `/photo`, `/status`, `/monitor_on`, `/timelapse_on` и др. |

## Железо

- Плата **ESP32-CAM (AI-Thinker)** с PSRAM
- Модуль камеры на шлейфе (чаще OV2640; поддерживаются и другие при включённых `CONFIG_*_SUPPORT` в `platformio.ini`)
- **MicroSD** в слоте платы (для таймлапса)
- Стабильное питание **5 V ≥ 2 A** (USB часто недостаточен при Wi‑Fi + камере)
- **Питание камеры с принтера** (режим по умолчанию): ESP32-CAM на том же БП/розетке, что и принтер. Каждое включение принтера = перезагрузка камеры → в Telegram: «Принтер включён» + снимок в **максимальном** качестве.

### Опционально: отдельный GPIO (если камера всегда под питанием)

Если ESP32 питается постоянно, а принтер включается отдельно — в `app_config.h` установите `PRINTER_POWER_GPIO_ENABLE` в `1` и подключите оптопару PC817 к **GPIO 33**.

### Вспышка

На AI-Thinker вспышка — **GPIO 4**. Перед каждым снимком (таймлапс, мониторинг, `/photo`) измеряется освещённость; если темно — LED включается на `FLASH_WARMUP_MS` мс. Порог: `FLASH_DARK_AVG_THRESHOLD` в `app_config.h`.

## Быстрый старт

1. Установите [PlatformIO](https://platformio.org/).
2. Скопируйте конфиг:
   ```bash
   cp include/secrets.example.h include/secrets.h
   ```
3. Заполните `WIFI_*`, `TELEGRAM_BOT_TOKEN`, `TELEGRAM_CHAT_ID`.
4. Соберите и прошейте:
   ```bash
   cd esp32-printer-cam
   pio run -t upload
   pio device monitor
   ```
5. В Serial увидите строку вида `[CAM] detected: OV2640 ...`, затем IP. Откройте в браузере.

### Telegram

1. Создайте бота через [@BotFather](https://t.me/BotFather), получите токен.
2. Напишите боту `/start`.
3. Узнайте `chat_id`:  
   `https://api.telegram.org/bot<TOKEN>/getUpdates`

## API (HTTP)

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/` | Веб-интерфейс |
| GET | `/stream` | MJPEG |
| GET | `/capture.jpg` | Один снимок |
| GET | `/api/status` | JSON статус |
| POST | `/api/timelapse/start` | `{"interval":30}` |
| POST | `/api/timelapse/stop` | Остановка |
| POST | `/api/monitor/start` | `{"interval":60}` |
| POST | `/api/monitor/stop` | Остановка |

## Команды Telegram

- `/help` — справка  
- `/photo` — снимок  
- `/status` — IP, камера, SD, режимы  
- `/monitor_on 60` — мониторинг, интервал в секундах  
- `/monitor_off`  
- `/timelapse_on 30` — таймлапс (нужна SD)  
- `/timelapse_off`  

## Настройка

Параметры в `include/app_config.h`:

- `STREAM_FRAMESIZE` — разрешение веб-потока (снимки — авто макс. сенсора)  
- `SNAPSHOT_QUALITY` — JPEG (4 ≈ максимум, меньше = лучше)  
- `TIMELAPSE_DEFAULT_INTERVAL_SEC` — интервал таймлапса  
- `MONITOR_STALL_MINUTES` — через сколько минут без движения слать предупреждение  
- `MONITOR_MOTION_THRESHOLD` — порог «движения» в %  
- `PRINTER_POWER_GPIO`, `PRINTER_POWER_DEBOUNCE_MS` — детект включения принтера  
- `FLASH_DARK_AVG_THRESHOLD`, `FLASH_BRIGHTNESS` — автовспышка  

## Сборка таймлапса в видео (на ПК)

Кадры лежат на SD в папке сессии. Скопируйте на компьютер и соберите, например:

```bash
ffmpeg -framerate 30 -pattern_type glob -i 'img_*.jpg' -c:v libx264 -pix_fmt yuv420p print.mp4
```

## Устранение неполадок

| Проблема | Решение |
|----------|---------|
| `Camera init failed` | Проверьте шлейф, прошивку с PSRAM, питание |
| `Detected camera not supported` | Обновите `esp32-camera`, добавьте `CONFIG_OVxxxx_SUPPORT=1` в `platformio.ini` |
| SD не монтируется | FAT32, карта ≤ 32 ГБ, вставлена до включения |
| Telegram не шлёт | Токен, chat_id, интернет; в Serial смотрите ошибки |
| Подвисания Wi‑Fi | Отдельный БП 5 V, антенна, роутер 2.4 GHz |

## Структура проекта

```
esp32-printer-cam/
├── platformio.ini
├── include/
│   ├── app_config.h
│   ├── secrets.example.h
│   └── secrets.h          ← не в git
└── src/
    ├── main.cpp
    ├── camera_module.*    ← автоинициализация камеры
    ├── sd_card.*
    ├── timelapse.*
    ├── print_monitor.*
    ├── printer_power.*
    ├── telegram_bot.*
    └── web_server.*
```

## Лицензия

MIT — используйте свободно в своих проектах.
