#include "web.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "credentials.h"
#include "event_render.h"
#include "event_store.h"
#include "log.h"
#include "net.h"
#include "psram_alloc.h"
#include "settings_store.h"
#include "updater.h"
#include "web_event_page.h"
#include "web_icon.h"
#include "web_page.h"

static const size_t MAX_BODY = 16384;  // a full settings JSON is ~5 KB, an event up to ~8 KB

static WebServer server(80);
static bool started = false;
static time_t lastFetch = 0;
static uint32_t restartAtMs = 0;  // 0: no restart requested
static uint32_t bootTag = 0;      // random per start, set in webBegin()

void webNoteFetch(time_t fetchedAt) { lastFetch = fetchedAt; }

// Sends `doc` from a PSRAM buffer (a String would take internal RAM).
static void sendJson(JsonDocument& doc) {
  size_t n = measureJson(doc);
  char* buf = (char*)heap_caps_malloc(n + 1, MALLOC_CAP_SPIRAM);
  if (!buf) {
    server.send(500, "text/plain", "out of memory");
    return;
  }
  serializeJson(doc, buf, n + 1);
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/json", buf, n);
  heap_caps_free(buf);
}

// Changes with every save and every start. The page compares it (via
// /api/status) to notice changes made on another device, and sends it along
// when saving, so it cannot overwrite those unseen.
static String settingsRev() { return String(bootTag, HEX) + "." + String(settingsVersion()); }

static void sendSettings() {
  JsonDocument doc(&spiRam);
  settingsToJson(settingsNet(), doc.to<JsonObject>(), true);
  doc["rev"] = settingsRev();
  sendJson(doc);
}

static void handleSave() {
  const String& body = server.arg("plain");
  if (body.length() == 0 || body.length() > MAX_BODY) {
    server.send(400, "text/plain", "leere oder zu große Anfrage");
    return;
  }
  // without ?rev= (a page from older firmware, or "save anyway") it just saves
  if (server.hasArg("rev") && server.arg("rev") != settingsRev()) {
    server.send(409, "text/plain", "inzwischen auf einem anderen Gerät geändert");
    return;
  }
  JsonDocument doc(&spiRam);
  DeserializationError err = deserializeJson(doc, body);
  Settings& s = settingsScratch();
  if (err || !settingsFromJson(doc.as<JsonVariantConst>(), s)) {
    server.send(400, "text/plain", String("ungültige Daten: ") + err.c_str());
    return;
  }
  if (!settingsSave(s)) {
    server.send(500, "text/plain", "Speichern im Flash fehlgeschlagen (gilt bis zum Neustart)");
    return;
  }
  sendSettings();  // the cleaned-up version, so the page shows what is in use
}

// ---- Event mode (src/event.h) -------------------------------------------------

static String eventRev() { return String(bootTag, HEX) + "." + String(eventVersion()); }

static void sendEvent() {
  JsonDocument doc(&spiRam);
  eventToJson(eventNet(), doc.to<JsonObject>());
  doc["rev"] = eventRev();
  doc["brightnessDefaultPct"] = BRIGHTNESS_DAY;
  sendJson(doc);
}

// Parses the request body into `doc`; sends the error and returns false if it can't.
static bool readBody(JsonDocument& doc) {
  const String& body = server.arg("plain");
  if (body.length() == 0 || body.length() > MAX_BODY) {
    server.send(400, "text/plain", "leere oder zu große Anfrage");
    return false;
  }
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "text/plain", String("ungültige Daten: ") + err.c_str());
    return false;
  }
  return true;
}

// The scenes and brightness from the event page, with ?rev= like
// handleSave(). Whether it is on stays as it is (/api/event/on).
static void handleEventSave() {
  if (server.hasArg("rev") && server.arg("rev") != eventRev()) {
    server.send(409, "text/plain", "inzwischen auf einem anderen Gerät geändert");
    return;
  }
  JsonDocument doc(&spiRam);
  if (!readBody(doc)) return;
  Event& e = eventScratch();
  if (!eventFromJson(doc.as<JsonVariantConst>(), e)) {
    server.send(400, "text/plain", "ungültige Daten");
    return;
  }
  e.on = eventNet().on;
  if (!eventSave(e)) {
    server.send(500, "text/plain", "Speichern im Flash fehlgeschlagen (gilt bis zum Neustart)");
    return;
  }
  sendEvent();
}

// {"on": true|false}: switches the event mode, the scenes stay as they are.
static void handleEventOn() {
  JsonDocument doc(&spiRam);
  if (!readBody(doc)) return;
  if (!doc["on"].is<bool>()) {
    server.send(400, "text/plain", "on fehlt");
    return;
  }
  Event& e = eventScratch();
  e = eventNet();
  e.on = doc["on"];
  if (!eventSave(e)) {
    server.send(500, "text/plain", "Speichern im Flash fehlgeschlagen (gilt bis zum Neustart)");
    return;
  }
  sendEvent();
}

// One scene (JSON as in the event) drawn as the board would show it ?ms=
// after it came up: 192 x 64 RGB565 pixels, little-endian, row by row. The
// page shows that, so its preview is exact without knowing the fonts.
static void handleEventPreview() {
  static GFXcanvas16* canvas = nullptr;  // 24 KB, in PSRAM (large mallocs go there)
  static U8G2_FOR_ADAFRUIT_GFX u8;
  if (!canvas) {
    canvas = new GFXcanvas16(PANEL_RES_X * PANEL_CHAIN, PANEL_RES_Y);
    if (!canvas->getBuffer()) {
      delete canvas;
      canvas = nullptr;
      server.send(500, "text/plain", "out of memory");
      return;
    }
    u8.begin(*canvas);
  }
  JsonDocument doc(&spiRam);
  if (!readBody(doc)) return;
  Scene sc;
  if (!sceneFromJson(doc.as<JsonVariantConst>(), sc)) {
    server.send(400, "text/plain", "ungültige Daten");
    return;
  }
  canvas->fillScreen(0);
  renderScene(*canvas, u8, sc, strtoul(server.arg("ms").c_str(), nullptr, 10));
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)canvas->getBuffer(),
                canvas->width() * canvas->height() * 2);
}

static void handleStatus() {
  time_t now = time(nullptr);
  JsonDocument doc(&spiRam);
  doc["build"] = firmwareBuild();
  doc["uptimeS"] = millis() / 1000;
  doc["dataAgeS"] = lastFetch && timeValid() ? (long)(now - lastFetch) : -1;
  doc["rev"] = settingsRev();
  doc["event"] = eventNet().on;
  doc["eventRev"] = eventRev();
  doc["off"] = !eventNet().on && timeValid() && isOffTime(settingsNet(), minuteOfDay(now));
  doc["rssi"] = WiFi.RSSI();
  doc["ssid"] = WiFi.SSID();
  doc["heap"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  doc["psram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  sendJson(doc);
}

// Log text after position `since` (see src/log.h). The page keeps polling
// with X-Log-Next; X-Log-Boot changes after a restart.
static void handleLog() {
  char* buf = (char*)heap_caps_malloc(LOG_BUFFER_BYTES, MALLOC_CAP_SPIRAM);
  if (!buf) {
    server.send(500, "text/plain", "out of memory");
    return;
  }
  uint32_t start;
  size_t n = logRead(strtoul(server.arg("since").c_str(), nullptr, 10), buf, LOG_BUFFER_BYTES, start);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Log-Boot", String(logBootId()));
  server.sendHeader("X-Log-Start", String(start));
  server.sendHeader("X-Log-Next", String(start + n));
  server.send_P(200, "text/plain; charset=utf-8", buf, n);
  heap_caps_free(buf);
}

// Restarts from webLoop() a moment later, so the reply still gets out. Saving
// settings, OTA and the update check all run in the network task like this
// handler, so none of them can be cut off half-way.
static void handleRestart() {
  server.send(200, "text/plain", "ok");
  restartAtMs = millis() + 500;
  if (!restartAtMs) restartAtMs = 1;
}

// New WiFi, used after the restart. A wrong one does no harm: the board
// falls back to the previous one (src/net.cpp). The password is never sent back.
static void handleWifi() {
  JsonDocument doc(&spiRam);
  const String& body = server.arg("plain");
  if (body.length() > 512 || deserializeJson(doc, body)) {
    server.send(400, "text/plain", "ungültige Daten");
    return;
  }
  const char* ssid = doc["ssid"] | "";
  const char* pass = doc["pass"] | "";
  size_t ns = strlen(ssid), np = strlen(pass);
  if (ns < 1 || ns > 32 || (np > 0 && np < 8) || np > 63) {
    server.send(400, "text/plain", "WLAN-Name 1–32 Zeichen, Passwort leer oder 8–63 Zeichen");
    return;
  }
  if (!credentialsSetWifi(ssid, pass)) {
    server.send(500, "text/plain", "Speichern im Flash fehlgeschlagen");
    return;
  }
  handleRestart();
}

// Browsers and iOS also ask for the fixed paths on their own, so all point here.
static void sendIcon() {
  server.sendHeader("Cache-Control", "max-age=604800");
  server.send_P(200, "image/png", (PGM_P)WEB_ICON_PNG, sizeof(WEB_ICON_PNG));
}

void webBegin() {
  if (started) return;
  bootTag = esp_random();  // WiFi is up, so this is truly random
  server.on("/", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");  // always the page of the running firmware
    server.send_P(200, "text/html; charset=utf-8", WEB_PAGE);
  });
  for (const char* path : {"/favicon.png", "/favicon.ico", "/apple-touch-icon.png", "/apple-touch-icon-precomposed.png"}) {
    server.on(path, HTTP_GET, sendIcon);
  }
  server.on("/api/settings", HTTP_GET, sendSettings);
  server.on("/api/settings", HTTP_POST, handleSave);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/event", HTTP_GET, [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", WEB_EVENT_PAGE);
  });
  server.on("/api/event", HTTP_GET, sendEvent);
  server.on("/api/event", HTTP_POST, handleEventSave);
  server.on("/api/event/on", HTTP_POST, handleEventOn);
  server.on("/api/event/preview", HTTP_POST, handleEventPreview);
  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
  server.begin();
  MDNS.addService("http", "tcp", 80);  // mDNS itself is started by ArduinoOTA
  started = true;
  Log.printf("[web] settings page at http://%s.local/ (%s)\n", HOSTNAME, WiFi.localIP().toString().c_str());
}

void webLoop() {
  if (started) server.handleClient();
  if (restartAtMs && (int32_t)(millis() - restartAtMs) >= 0) {
    Log.println("[web] restart requested, rebooting");
    delay(100);
    ESP.restart();
  }
}
