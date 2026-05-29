#include "web_server.h"
#include "app_config.h"
#include "camera_module.h"
#include "print_monitor.h"
#include "sd_card.h"
#include "printer_power.h"
#include "timelapse.h"
#include <WiFi.h>
#include <esp_camera.h>

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
header{padding:12px 16px;background:#1e293b}main{padding:16px;max-width:900px;margin:auto}
img,video{max-width:100%;border-radius:8px;background:#000}
.card{background:#1e293b;border-radius:10px;padding:14px;margin:12px 0}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
button,input{padding:8px 12px;border-radius:6px;border:none}
button{background:#3b82f6;color:#fff;cursor:pointer}button.off{background:#64748b}
label{font-size:.9rem;color:#94a3b8}
</style>
</head>
<body>
<header><h1>🖨 ESP32 Printer Cam</h1></header>
<main>
<div class="card"><img id="stream" src="/stream" alt="stream"></div>
<div class="card">
<h3>Таймлапс (SD)</h3>
<div class="row">
<button id="tlOn">Старт</button><button id="tlOff" class="off">Стоп</button>
<label>Интервал (сек)</label><input id="tlInt" type="number" min="5" value="30" style="width:80px">
</div>
<pre id="tlStatus"></pre>
</div>
<div class="card">
<h3>Мониторинг печати (Telegram)</h3>
<div class="row">
<button id="monOn">Старт</button><button id="monOff" class="off">Стоп</button>
<label>Интервал (сек)</label><input id="monInt" type="number" min="10" value="60" style="width:80px">
</div>
<pre id="monStatus"></pre>
</div>
<div class="card"><pre id="sysStatus"></pre></div>
</main>
<script>
async function api(path,opts){const r=await fetch(path,opts);return r.json()}
async function refresh(){
  const s=await api('/api/status');
  document.getElementById('sysStatus').textContent=JSON.stringify(s,null,2);
  document.getElementById('tlStatus').textContent=JSON.stringify(s.timelapse,null,2);
  document.getElementById('monStatus').textContent=JSON.stringify(s.monitor,null,2);
}
document.getElementById('tlOn').onclick=async()=>{
  await api('/api/timelapse/start',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({interval:+document.getElementById('tlInt').value})});refresh()};
document.getElementById('tlOff').onclick=async()=>{await api('/api/timelapse/stop',{method:'POST'});refresh()};
document.getElementById('monOn').onclick=async()=>{
  await api('/api/monitor/start',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({interval:+document.getElementById('monInt').value})});refresh()};
document.getElementById('monOff').onclick=async()=>{await api('/api/monitor/stop',{method:'POST'});refresh()};
setInterval(refresh,3000);refresh();
</script>
</body>
</html>
)rawliteral";

static void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStream() {
    WiFiClient client = server.client();
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
    server.sendContent(response);

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, STREAM_FRAMESIZE);
        s->set_quality(s, STREAM_QUALITY);
    }

    while (client.connected()) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            delay(10);
            continue;
        }
        client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");
        esp_camera_fb_return(fb);
        delay(1);
    }
}

static void handleCapture() {
    uint8_t *buf = nullptr;
    size_t len = 0;
    if (!cameraCaptureSnapshot(&buf, &len, false)) {
        server.send(500, "text/plain", "capture failed");
        return;
    }
    server.send_P(200, "image/jpeg", (const char *)buf, len);
    free(buf);
}

static void handleStatus() {
    String json = "{";
    json += "\"camera\":\"" + String(cameraSensorName()) + "\"";
    json += ",\"snapshot\":\"" + String(cameraSnapshotResolutionName()) + "\"";
    json += ",\"snapshot_quality\":" + String(cameraSnapshotQuality());
    json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
    json += ",\"heap\":" + String(ESP.getFreeHeap());
    json += ",\"sd_free_mb\":" + String(sdReady() ? sdFreeBytes() / (1024 * 1024) : 0);
    json += ",\"timelapse\":" + timelapseStatusJson();
    json += ",\"monitor\":" + monitorStatusJson();
    json += ",\"printer_power\":" + String(printerPowerIsOn() ? "true" : "false");
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
    server.on("/stream", HTTP_GET, handleStream);
    server.on("/capture.jpg", HTTP_GET, handleCapture);
    server.on("/api/status", HTTP_GET, handleStatus);

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
    Serial.printf("[WEB] http://%s/\n", WiFi.localIP().toString().c_str());
}

void webLoop() { server.handleClient(); }
WebServer &webInstance() { return server; }
