#include "camera_control.h"
#include "app_config.h"
#include "camera_module.h"
#include <esp_camera.h>

bool cameraControlSet(const char *var, int val) {
    sensor_t *s = esp_camera_sensor_get();
    if (!s || !var) return false;

    if (!strcmp(var, "lamp")) {
        cameraSetLampPercent(constrain(val, 0, 100));
        return true;
    }
    if (!strcmp(var, "autolamp")) {
        cameraSetStreamAutolamp(val != 0);
        return true;
    }
    if (!strcmp(var, "framesize")) {
        if (val >= 0 && val <= (int)FRAMESIZE_5MP) {
            s->set_framesize(s, (framesize_t)val);
            return true;
        }
        return false;
    }
    if (!strcmp(var, "quality")) {
        s->set_quality(s, constrain(val, 4, 63));
        return true;
    }
    if (!strcmp(var, "brightness")) return s->set_brightness(s, val) == 0;
    if (!strcmp(var, "contrast")) return s->set_contrast(s, val) == 0;
    if (!strcmp(var, "saturation")) return s->set_saturation(s, val) == 0;
    if (!strcmp(var, "hmirror")) return s->set_hmirror(s, val) == 0;
    if (!strcmp(var, "vflip")) return s->set_vflip(s, val) == 0;
    if (!strcmp(var, "awb")) return s->set_whitebal(s, val) == 0;
    if (!strcmp(var, "aec")) return s->set_exposure_ctrl(s, val) == 0;
    if (!strcmp(var, "agc")) return s->set_gain_ctrl(s, val) == 0;

    return false;
}

String cameraSensorStatusJson() {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) return "{}";

    String j = "{";
    j += "\"framesize\":" + String((int)s->status.framesize);
    j += ",\"quality\":" + String(s->status.quality);
    j += ",\"brightness\":" + String(s->status.brightness);
    j += ",\"contrast\":" + String(s->status.contrast);
    j += ",\"saturation\":" + String(s->status.saturation);
    j += ",\"hmirror\":" + String(s->status.hmirror);
    j += ",\"vflip\":" + String(s->status.vflip);
    j += ",\"lamp\":" + String(cameraGetLampPercent());
    j += ",\"autolamp\":" + String(cameraStreamAutolamp() ? 1 : 0);
    j += "}";
    return j;
}
