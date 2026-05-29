#include "camera_module.h"
#include "app_config.h"

static CameraInfo g_info;
static bool g_flashPwmReady = false;

void cameraFlashInit() {
    pinMode(FLASH_LED_GPIO, OUTPUT);
    digitalWrite(FLASH_LED_GPIO, LOW);
    g_flashPwmReady = false;
}

void cameraFlashSet(uint8_t brightness) {
    if (!g_flashPwmReady) {
        ledcSetup(FLASH_PWM_CHANNEL, FLASH_PWM_FREQ_HZ, FLASH_PWM_RES_BITS);
        ledcAttachPin(FLASH_LED_GPIO, FLASH_PWM_CHANNEL);
        g_flashPwmReady = true;
    }
    ledcWrite(FLASH_PWM_CHANNEL, brightness);
    if (brightness == 0) {
        digitalWrite(FLASH_LED_GPIO, LOW);
    }
}

static camera_config_t aiThinkerConfig() {
    camera_config_t cfg = {};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer = LEDC_TIMER_0;
    cfg.pin_d0 = 5;
    cfg.pin_d1 = 18;
    cfg.pin_d2 = 19;
    cfg.pin_d3 = 21;
    cfg.pin_d4 = 36;
    cfg.pin_d5 = 39;
    cfg.pin_d6 = 34;
    cfg.pin_d7 = 35;
    cfg.pin_xclk = 0;
    cfg.pin_pclk = 22;
    cfg.pin_vsync = 25;
    cfg.pin_href = 23;
    cfg.pin_sccb_sda = 26;
    cfg.pin_sccb_scl = 27;
    cfg.pin_pwdn = 32;
    cfg.pin_reset = -1;
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

    camera_config_t cfg = aiThinkerConfig();
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

    Serial.printf("[CAM] detected: %s PID=0x%04x snapshot=%d quality=%d\n",
                  info.name, info.sensorId.PID, (int)info.maxFrameSize, SNAPSHOT_QUALITY);
    return true;
}

framesize_t cameraSnapshotSize() {
    return g_info.ok ? g_info.maxFrameSize : FRAMESIZE_UXGA;
}

int cameraSnapshotQuality() {
    return SNAPSHOT_QUALITY;
}

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
        case FRAMESIZE_P_HD: return "720x1280";
        case FRAMESIZE_P_3MP: return "864x1536";
        case FRAMESIZE_QXGA: return "2048x1536";
        case FRAMESIZE_QHD: return "2560x1440";
        case FRAMESIZE_WQXGA: return "2560x1600";
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
    for (size_t i = 0; i < 64; i++) {
        sum += fb->buf[i * step];
    }
    uint8_t avg = (uint8_t)(sum / 64);
    esp_camera_fb_return(fb);

    s->set_pixformat(s, PIXFORMAT_JPEG);
    s->set_framesize(s, prevSize);

    Serial.printf("[CAM] ambient avg=%u (threshold %d)\n", avg, FLASH_DARK_AVG_THRESHOLD);
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
    if (autoFlash) {
        useFlash = cameraIsDarkScene();
    }
#else
    (void)autoFlash;
#endif

    if (useFlash) {
        cameraFlashSet(FLASH_BRIGHTNESS);
        delay(FLASH_WARMUP_MS);
        esp_camera_fb_return(esp_camera_fb_get());
    }

    bool ok = captureFrame(s, size, quality, buf, len);

    if (useFlash) {
        cameraFlashSet(0);
    }

    return ok;
}

void cameraReleaseFrame(camera_fb_t *fb) {
    if (fb) esp_camera_fb_return(fb);
}

const char *cameraSensorName() {
    return g_info.ok ? g_info.name : "none";
}
