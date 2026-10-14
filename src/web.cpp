#include "web.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "credentials.h"
#include "net.h"
#include "psram_alloc.h"
#include "settings_store.h"
#include "updater.h"
#include "web_icon.h"
#include "web_page.h"

static const size_t MAX_BODY = 16384;  // a full settings JSON is ~5 KB

static WebServer server(80);
static bool started = false;
static time_t lastFetch = 0;
static uint32_t restartAtMs = 0;  // 0: no restart requested

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
  server.send_P(200, "application/json", buf, n);
  heap_caps_free(buf);
}

static void sendSettings() {
  JsonDocument doc(&spiRam);
  settingsToJson(settingsNet(), doc.to<JsonObject>(), true);
  sendJson(doc);
}

static void handleSave() {
  const String& body = server.arg("plain");
  if (body.length() == 0 || body.length() > MAX_BODY) {
    server.send(400, "text/plain", "leere oder zu große Anfrage");
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

static void handleStatus() {
  time_t now = time(nullptr);
  JsonDocument doc(&spiRam);
  doc["build"] = firmwareBuild();
  doc["uptimeS"] = millis() / 1000;
  doc["dataAgeS"] = lastFetch && timeValid() ? (long)(now - lastFetch) : -1;
  doc["off"] = timeValid() && isOffTime(settingsNet(), minuteOfDay(now));
  doc["rssi"] = WiFi.RSSI();
  doc["ssid"] = WiFi.SSID();
  doc["heap"] = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  doc["psram"] = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  sendJson(doc);
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
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  for (const char* path : {"/favicon.png", "/favicon.ico", "/apple-touch-icon.png", "/apple-touch-icon-precomposed.png"}) {
    server.on(path, HTTP_GET, sendIcon);
  }
  server.on("/api/settings", HTTP_GET, sendSettings);
  server.on("/api/settings", HTTP_POST, handleSave);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
  server.begin();
  MDNS.addService("http", "tcp", 80);  // mDNS itself is started by ArduinoOTA
  started = true;
  Serial.printf("[web] settings page at http://%s.local/ (%s)\n", HOSTNAME, WiFi.localIP().toString().c_str());
}

void webLoop() {
  if (started) server.handleClient();
  if (restartAtMs && (int32_t)(millis() - restartAtMs) >= 0) {
    Serial.println("[web] restart requested, rebooting");
    delay(100);
    ESP.restart();
  }
}
