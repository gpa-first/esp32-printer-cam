#include "settings_store.h"
#include "app_config.h"
#include "camera_module.h"
#include "print_monitor.h"
#include "timelapse.h"
#include <Preferences.h>

static Preferences g_prefs;
static const char *NS = "prtcam";

void settingsLoad() {
    if (!g_prefs.begin(NS, false)) return;

    int lamp = g_prefs.getInt("lamp", FLASH_LAMP_PERCENT);
    cameraSetLampPercent(lamp);

    uint32_t tl = g_prefs.getUInt("tl_int", TIMELAPSE_DEFAULT_INTERVAL_SEC);
    timelapseSetIntervalSec(tl, false);

    uint32_t mon = g_prefs.getUInt("mon_int", MONITOR_DEFAULT_INTERVAL_SEC);
    monitorSetIntervalSec(mon, false);

    float mot = g_prefs.getFloat("mot_thr", MONITOR_MOTION_THRESHOLD);
    monitorSetMotionThreshold(mot, false);

    g_prefs.end();
    Serial.println("[CFG] settings loaded from NVS");
}

void settingsSave() {
    if (!g_prefs.begin(NS, false)) return;

    g_prefs.putInt("lamp", cameraGetLampPercent());
    g_prefs.putUInt("tl_int", timelapseIntervalSec());
    g_prefs.putUInt("mon_int", monitorIntervalSec());
    g_prefs.putFloat("mot_thr", monitorMotionThreshold());

    g_prefs.end();
    Serial.println("[CFG] settings saved to NVS");
}
