#include "camera_module.h"
#include "app_config.h"
#include "camera_pins.h"
#include <math.h>

static CameraInfo g_info;
static bool g_flashPwmReady = false;
static int g_lampPercent = FLASH_LAMP_PERCENT;
static bool g_streamAutolamp = STREAM_AUTOLAMP;
static volatile bool g_streamActive = false;
static volatile bool g_streamKill = false;

static int lampPercentToPwm(int percent) {
    if (percent <= 0) return 0;
    const int pwmMax = (1 << FLASH_PWM_RES_BITS) - 1;
    double v = (pow(2.0, (1.0 + (percent * 0.02))) - 2.0) / 6.0 * pwmMax;
    return constrain((int)round(v), 0, pwmMax);
}

void cameraFlashInit() {
#if LAMP_PIN >= 0
    pinMode(LAMP_PIN, OUTPUT);
    digitalWrite(LAMP_PIN, LOW);
#endif
#if defined(STATUS_LED_GPIO) && STATUS_LED_GPIO >= 0
    pinMode(STATUS_LED_GPIO, OUTPUT);
    digitalWrite(STATUS_LED_GPIO, !STATUS_LED_ON);
#endif
    g_flashPwmReady = false;
}

void cameraFlashSet(uint8_t pwmRaw) {
#if LAMP_PIN < 0
    (void)pwmRaw;
    return;
#else
    if (!g_flashPwmReady) {
        ledcSetup(FLASH_PWM_CHANNEL, FLASH_PWM_FREQ_HZ, FLASH_PWM_RES_BITS);
        ledcAttachPin(LAMP_PIN, FLASH_PWM_CHANNEL);
        g_flashPwmReady = true;
    }
    ledcWrite(FLASH_PWM_CHANNEL, pwmRaw);
    if (pwmRaw == 0) {
        digitalWrite(LAMP_PIN, LOW);
    }
#endif
}

void cameraFlashSetPercent(int percent) {
    cameraFlashSet((uint8_t)lampPercentToPwm(constrain(percent, 0, 100)));
}

int cameraGetLampPercent() { return g_lampPercent; }
void cameraSetLampPercent(int percent) { g_lampPercent = constrain(percent, 0, 100); }

void cameraSetStreamAutolamp(bool on) { g_streamAutolamp = on; }
bool cameraStreamAutolamp() { return g_streamAutolamp; }

void cameraStreamLampOn() {
#if STREAM_AUTOLAMP && LAMP_PIN >= 0
    if (g_streamAutolamp) {
        cameraFlashSetPercent(STREAM_LAMP_PERCENT);
    }
#endif
}

void cameraStreamLampOff() {
#if LAMP_PIN >= 0
    cameraFlashSet(0);
#endif
}

void cameraStatusLedFlash(int ms) {
#if defined(STATUS_LED_GPIO) && STATUS_LED_GPIO >= 0
    digitalWrite(STATUS_LED_GPIO, STATUS_LED_ON);
    delay(ms);
    digitalWrite(STATUS_LED_GPIO, !STATUS_LED_ON);
#endif
}

bool cameraStreamActive() { return g_streamActive; }
void cameraStreamSetActive(bool active) { g_streamActive = active; }
void cameraStreamReset() { g_streamKill = false; }
void cameraStreamRequestStop() { g_streamKill = true; }
bool cameraStreamShouldStop() { return g_streamKill; }

static camera_config_t boardCameraConfig() {
    camera_config_t cfg = {};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer = LEDC_TIMER_0;
    cfg.pin_d0 = Y2_GPIO_NUM;
    cfg.pin_d1 = Y3_GPIO_NUM;
    cfg.pin_d2 = Y4_GPIO_NUM;
    cfg.pin_d3 = Y5_GPIO_NUM;
    cfg.pin_d4 = Y6_GPIO_NUM;
    cfg.pin_d5 = Y7_GPIO_NUM;
    cfg.pin_d6 = Y8_GPIO_NUM;
    cfg.pin_d7 = Y9_GPIO_NUM;
    cfg.pin_xclk = XCLK_GPIO_NUM;
    cfg.pin_pclk = PCLK_GPIO_NUM;
    cfg.pin_vsync = VSYNC_GPIO_NUM;
    cfg.pin_href = HREF_GPIO_NUM;
    cfg.pin_sccb_sda = SIOD_GPIO_NUM;
    cfg.pin_sccb_scl = SIOC_GPIO_NUM;
    cfg.pin_pwdn = PWDN_GPIO_NUM;
    cfg.pin_reset = RESET_GPIO_NUM;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size = FRAMESIZE_UXGA;
    cfg.jpeg_quality = SNAPSHOT_QUALITY;
    cfg.fb_count = 2;
    cfg.grab_mode = CAMERA_GRAB_LATEST;
    cfg.fb_location = CAMERA_FB_IN_PSRAM;
    return cfg;
}

bool cameraInitAuto(CameraInfo &info) {
    cameraFlashInit();

    camera_config_t cfg = boardCameraConfig();
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("[CAM] init failed: 0x%x\n", err);
        info.ok = false;
        return false;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
        info.ok = false;
        return false;
    }

    info.sensorId = s->id;
    info.ok = true;

    camera_sensor_info_t *si = esp_camera_sensor_get_info(&s->id);
    if (si) {
        info.name = si->name;
        info.maxFrameSize = si->max_framesize;
    } else {
        info.name = "unknown";
        info.maxFrameSize = FRAMESIZE_UXGA;
    }

    g_info = info;

    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 0);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_gainceiling(s, (gainceiling_t)6);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 1);

    s->set_framesize(s, info.maxFrameSize);
    s->set_quality(s, SNAPSHOT_QUALITY);

    Serial.printf("[CAM] %s PID=0x%04x snapshot=%d quality=%d\n",
                  info.name, info.sensorId.PID, (int)info.maxFrameSize, SNAPSHOT_QUALITY);
    return true;
}

framesize_t cameraSnapshotSize() {
    return g_info.ok ? g_info.maxFrameSize : FRAMESIZE_UXGA;
}

int cameraSnapshotQuality() { return SNAPSHOT_QUALITY; }

const char *cameraSnapshotResolutionName() {
    switch (cameraSnapshotSize()) {
        case FRAMESIZE_QQVGA: return "160x120";
        case FRAMESIZE_QVGA: return "320x240";
        case FRAMESIZE_VGA: return "640x480";
        case FRAMESIZE_SVGA: return "800x600";
        case FRAMESIZE_XGA: return "1024x768";
        case FRAMESIZE_HD: return "1280x720";
        case FRAMESIZE_SXGA: return "1280x1024";
        case FRAMESIZE_UXGA: return "1600x1200";
        case FRAMESIZE_FHD: return "1920x1080";
        case FRAMESIZE_QXGA: return "2048x1536";
        case FRAMESIZE_5MP: return "2592x1944";
        default: return "max";
    }
}

static void discardFrames(int count) {
    for (int i = 0; i < count; i++) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }
}

bool cameraCaptureSnapshot(uint8_t **buf, size_t *len, bool autoFlash) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;

    framesize_t sz = cameraSnapshotSize();
    int q = cameraSnapshotQuality();
    s->set_framesize(s, sz);
    s->set_quality(s, q);
    discardFrames(SNAPSHOT_WARMUP_FRAMES);

    return cameraCaptureJpeg(sz, q, buf, len, autoFlash);
}

bool cameraIsDarkScene() {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;

    framesize_t prevSize = s->status.framesize;
    s->set_framesize(s, FRAMESIZE_96X96);
    s->set_pixformat(s, PIXFORMAT_GRAYSCALE);

    esp_camera_fb_return(esp_camera_fb_get());

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb || !fb->buf || fb->len == 0) {
        if (fb) esp_camera_fb_return(fb);
        s->set_pixformat(s, PIXFORMAT_JPEG);
        s->set_framesize(s, prevSize);
        return false;
    }

    uint32_t sum = 0;
    size_t step = max((size_t)1, fb->len / 64);
    for (size_t i = 0; i < 64; i++) sum += fb->buf[i * step];
    uint8_t avg = (uint8_t)(sum / 64);
    esp_camera_fb_return(fb);

    s->set_pixformat(s, PIXFORMAT_JPEG);
    s->set_framesize(s, prevSize);

    return avg < FLASH_DARK_AVG_THRESHOLD;
}

static bool captureFrame(sensor_t *s, framesize_t size, int quality,
                         uint8_t **buf, size_t *len) {
    framesize_t prevSize = s->status.framesize;
    int prevQ = s->status.quality;

    s->set_framesize(s, size);
    s->set_quality(s, quality);

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb || fb->format != PIXFORMAT_JPEG) {
        if (fb) esp_camera_fb_return(fb);
        s->set_framesize(s, prevSize);
        s->set_quality(s, prevQ);
        return false;
    }

    *buf = (uint8_t *)malloc(fb->len);
    if (!*buf) {
        esp_camera_fb_return(fb);
        s->set_framesize(s, prevSize);
        s->set_quality(s, prevQ);
        return false;
    }
    memcpy(*buf, fb->buf, fb->len);
    *len = fb->len;
    esp_camera_fb_return(fb);

    s->set_framesize(s, prevSize);
    s->set_quality(s, prevQ);
    return true;
}

bool cameraCaptureJpeg(framesize_t size, int quality, uint8_t **buf, size_t *len,
                       bool autoFlash) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return false;

    bool useFlash = false;
#if FLASH_AUTO_ENABLE
    if (autoFlash) useFlash = cameraIsDarkScene();
#else
    (void)autoFlash;
#endif

    if (useFlash) {
        cameraFlashSetPercent(g_lampPercent > 0 ? g_lampPercent : FLASH_LAMP_PERCENT);
        delay(FLASH_WARMUP_MS);
        discardFrames(1);
    }

    bool ok = captureFrame(s, size, quality, buf, len);

    if (useFlash) cameraFlashSet(0);

    return ok;
}

void cameraReleaseFrame(camera_fb_t *fb) {
    if (fb) esp_camera_fb_return(fb);
}

const char *cameraSensorName() {
    return g_info.ok ? g_info.name : "none";
}
