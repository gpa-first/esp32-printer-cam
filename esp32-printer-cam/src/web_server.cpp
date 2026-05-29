#include "web_server.h"
#include "app_config.h"
#include "camera_control.h"
#include "camera_module.h"
#include "print_monitor.h"
#include "printer_power.h"
#include "sd_card.h"
#include "system_info.h"
#include "timelapse.h"
#include <ESP.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_timer.h>

static WebServer server(HTTP_PORT);

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Printer Cam</title>
<style>
*{box-sizing:border-box}body{font-family:system-ui,sans-serif;margin:0;background:#111;color:#eee}
header{padding:12px 16px;background:#1e293b;display:flex;flex-wrap:wrap;gap:8px;align-items:center}
header a{color:#93c5fd;margin-right:12px}main{padding:16px;max-width:960px;margin:auto}
img{max-width:100%;border-radius:8px;background:#000}
.card{background:#1e293b;border-radius:10px;padding:14px;margin:12px 0}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
button,input,select{padding:8px 12px;border-radius:6px;border:none}
button{background:#3b82f6;color:#fff;cursor:pointer}button.off{background:#64748b}
label{font-size:.9rem;color:#94a3b8}
</style>
</head>
<body>
<header>
<h1 style="margin:0">🖨 Printer Cam</h1>
<a href="/viewer">Просмотр</a>
<a href="/capture.jpg" target="_blank">Снимок</a>
<a href="/dump">Диагностика</a>
</header>
<main>
<div class="card">
<img id="stream" src="/stream" alt="stream">
<div class="row" style="margin-top:8px">
<button onclick="fetch('/stop')">Стоп стрим</button>
<label>Зеркало</label><button onclick="ctl('hmirror',1)">H</button><button onclick="ctl('vflip',1)">V</button>
<label>Вспышка %</label><input id="lamp" type="range" min="0" max="100" value="30" onchange="ctl('lamp',this.value)">
</div>
</div>
<div class="card"><h3>Таймлапс (SD)</h3>
<div class="row">
<button id="tlOn">Старт</button><button id="tlOff" class="off">Стоп</button>
<label>Интервал (сек)</label><input id="tlInt" type="number" min="5" value="30" style="width:80px">
</div><pre id="tlStatus"></pre></div>
<div class="card"><h3>Мониторинг (Telegram)</h3>
<div class="row">
<button id="monOn">Старт</button><button id="monOff" class="off">Стоп</button>
<label>Интервал (сек)</label><input id="monInt" type="number" min="10" value="60" style="width:80px">
</div><pre id="monStatus"></pre></div>
<div class="card"><pre id="sysStatus"></pre></div>
</main>
<script>
async function api(p,o){return (await fetch(p,o)).json()}
function ctl(v,val){fetch('/control?var='+v+'&val='+val)}
async function refresh(){
  const s=await api('/api/status');
  document.getElementById('sysStatus').textContent=JSON.stringify(s,null,2);
  document.getElementById('tlStatus').textContent=JSON.stringify(s.timelapse,null,2);
  document.getElementById('monStatus').textContent=JSON.stringify(s.monitor,null,2);
  if(s.sensor&&s.sensor.lamp!=null)document.getElementById('lamp').value=s.sensor.lamp;
}
document.getElementById('tlOn').onclick=async()=>{await api('/api/timelapse/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({interval:+document.getElementById('tlInt').value})});refresh()};
document.getElementById('tlOff').onclick=async()=>{await api('/api/timelapse/stop',{method:'POST'});refresh()};
document.getElementById('monOn').onclick=async()=>{await api('/api/monitor/start',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({interval:+document.getElementById('monInt').value})});refresh()};
document.getElementById('monOff').onclick=async()=>{await api('/api/monitor/stop',{method:'POST'});refresh()};
setInterval(refresh,3000);refresh();
</script>
</body>
</html>
)rawliteral";

static const char VIEWER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Stream</title>
<style>
html,body{margin:0;height:100%;background:#000;overflow:hidden}
img{width:100%;height:100%;object-fit:contain}
</style></head>
<body>
<img id="s" src="/stream" alt="stream">
<script>
const i=document.getElementById('s');
document.ondblclick=()=>{if(!document.fullscreenElement)document.documentElement.requestFullscreen();else document.exitFullscreen()};
</script>
</body></html>
)rawliteral";

static void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handleViewer() {
    server.send_P(200, "text/html", VIEWER_HTML);
}

static void handleStream() {
    if (cameraStreamActive()) {
        server.send(503, "text/plain", "Stream busy (one client at a time)");
        return;
    }

    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
    response += "Access-Control-Allow-Origin: *\r\n\r\n";
    server.sendContent(response);

    cameraStreamReset();
    cameraStreamSetActive(true);
    cameraStatusLedFlash(75);
    delay(75);
    cameraStatusLedFlash(75);
    cameraStreamLampOn();

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, STREAM_FRAMESIZE);
        s->set_quality(s, STREAM_QUALITY);
    }

    int64_t lastFrameUs = esp_timer_get_time();

    while (client.connected() && !cameraStreamShouldStop()) {
        int64_t frameStart = esp_timer_get_time();

        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            delay(10);
            continue;
        }
        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");
        esp_camera_fb_return(fb);

        int64_t frameMs = (esp_timer_get_time() - frameStart) / 1000;
        int32_t delayMs = STREAM_MIN_FRAME_MS - (int32_t)frameMs;
        if (delayMs > 0) delay(delayMs);

        lastFrameUs = esp_timer_get_time();
        (void)lastFrameUs;
    }

    cameraStreamLampOff();
    cameraStreamSetActive(false);

    sensor_t *s2 = esp_camera_sensor_get();
    if (s2) {
        s2->set_framesize(s2, cameraSnapshotSize());
        s2->set_quality(s2, cameraSnapshotQuality());
    }
}

static void handleCapture() {
    cameraStatusLedFlash(50);
    uint8_t *buf = nullptr;
    size_t len = 0;
    if (!cameraCaptureSnapshot(&buf, &len, true)) {
        server.send(500, "text/plain", "capture failed");
        return;
    }
    server.send_P(200, "image/jpeg", (const char *)buf, len);
    free(buf);
}

static void handleStop() {
    cameraStreamRequestStop();
    server.send(200, "text/plain", "stream stop requested");
}

static void handleSignal() {
    String json = "{\"rssi\":" + String(WiFi.RSSI()) +
                  ",\"ssid\":\"" + WiFi.SSID() + "\"}";
    server.send(200, "application/json", json);
}

static void handleReboot() {
    server.send(200, "text/plain", "rebooting...");
    delay(200);
    ESP.restart();
}

static void handleControl() {
    if (!server.hasArg("var")) {
        server.send(400, "text/plain", "var required");
        return;
    }
    String var = server.arg("var");
    int val = server.hasArg("val") ? server.arg("val").toInt() : 0;
    if (cameraControlSet(var.c_str(), val)) {
        server.send(200, "text/plain", "ok");
    } else {
        server.send(400, "text/plain", "unknown var");
    }
}

static void handleDump() {
    String d;
    d += "<html><head><meta charset=utf-8><title>Dump</title></head><body><pre>";
    d += "ESP32 Printer Cam\n";
    d += "Camera: " + String(cameraSensorName()) + "\n";
    d += "Snapshot: " + String(cameraSnapshotResolutionName()) + " Q" + String(cameraSnapshotQuality()) + "\n";
    d += "IP: " + WiFi.localIP().toString() + " RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    d += "Heap: " + String(ESP.getFreeHeap()) + " / " + String(ESP.getHeapSize()) + "\n";
    d += "Uptime: " + String(millis() / 1000) + " s\n";
    d += "Chip temp: " + String(systemChipTempC(), 1) + " C\n";
    d += "Stream active: " + String(cameraStreamActive() ? "yes" : "no") + "\n";
    d += "SD: " + String(sdReady() ? "ok" : "no");
    if (sdReady()) {
        d += " free " + String(sdFreeBytes() / (1024 * 1024)) + " MB\n";
    } else {
        d += "\n";
    }
    d += "\nSensor:\n" + cameraSensorStatusJson();
    d += "\n</pre></body></html>";
    server.send(200, "text/html", d);
}

static void handleStatus() {
    String json = "{";
    json += "\"name\":\"" + String(CAM_NAME) + "\"";
    json += ",\"version\":\"" + String(FIRMWARE_VERSION) + "\"";
    json += ",\"camera\":\"" + String(cameraSensorName()) + "\"";
    json += ",\"snapshot\":\"" + String(cameraSnapshotResolutionName()) + "\"";
    json += ",\"snapshot_quality\":" + String(cameraSnapshotQuality());
    json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += ",\"rssi\":" + String(WiFi.RSSI());
    json += ",\"heap\":" + String(ESP.getFreeHeap());
    json += ",\"stream_active\":" + String(cameraStreamActive() ? "true" : "false");
    json += ",\"sd_free_mb\":" + String(sdReady() ? sdFreeBytes() / (1024 * 1024) : 0);
    json += ",\"sensor\":" + cameraSensorStatusJson();
    json += ",\"timelapse\":" + timelapseStatusJson();
    json += ",\"monitor\":" + monitorStatusJson();
    json += ",\"printer_power\":" + String(printerPowerIsOn() ? "true" : "false");
    json += ",\"system\":" + systemInfoJson();
    json += ",\"stream_url\":\"http://" + WiFi.localIP().toString() + "/stream\"";
    json += "}";
    server.send(200, "application/json", json);
}

static void parseIntervalFromBody(uint32_t &out) {
    if (!server.hasArg("plain")) return;
    int i = server.arg("plain").indexOf("interval");
    if (i < 0) return;
    int colon = server.arg("plain").indexOf(':', i);
    if (colon < 0) return;
    out = server.arg("plain").substring(colon + 1).toInt();
}

void webInit() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/viewer", HTTP_GET, handleViewer);
    server.on("/stream", HTTP_GET, handleStream);
    server.on("/capture.jpg", HTTP_GET, handleCapture);
    server.on("/capture", HTTP_GET, handleCapture);
    server.on("/stop", HTTP_GET, handleStop);
    server.on("/control", HTTP_GET, handleControl);
    server.on("/dump", HTTP_GET, handleDump);
    server.on("/signal", HTTP_GET, handleSignal);
    server.on("/reboot", HTTP_GET, handleReboot);
    server.on("/api/status", HTTP_GET, handleStatus);

    server.on("/esp32_cam_main", HTTP_GET, handleRoot);
    server.on("/esp32_cam_stream", HTTP_GET, handleStream);
    server.on("/esp32_cam_capture", HTTP_GET, handleCapture);

    server.on("/api/timelapse/start", HTTP_POST, []() {
        uint32_t sec = timelapseIntervalSec();
        parseIntervalFromBody(sec);
        timelapseSetIntervalSec(sec);
        timelapseSetEnabled(true);
        server.send(200, "application/json", timelapseStatusJson());
    });
    server.on("/api/timelapse/stop", HTTP_POST, []() {
        timelapseSetEnabled(false);
        server.send(200, "application/json", timelapseStatusJson());
    });
    server.on("/api/monitor/start", HTTP_POST, []() {
        uint32_t sec = monitorIntervalSec();
        parseIntervalFromBody(sec);
        monitorSetIntervalSec(sec);
        monitorSetEnabled(true);
        server.send(200, "application/json", monitorStatusJson());
    });
    server.on("/api/monitor/stop", HTTP_POST, []() {
        monitorSetEnabled(false);
        server.send(200, "application/json", monitorStatusJson());
    });

    server.begin();
    Serial.printf("[WEB] http://%s/  viewer: /viewer\n", WiFi.localIP().toString().c_str());
}

void webLoop() { server.handleClient(); }
WebServer &webInstance() { return server; }
