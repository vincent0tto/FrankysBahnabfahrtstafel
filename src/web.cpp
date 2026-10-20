#include "web.h"

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "credentials.h"
#include "event_render.h"
#include "event_store.h"
#include "image_store.h"
#include "karaoke_render.h"
#include "log.h"
#include "net.h"
#include "parse.h"
#include "psram_alloc.h"
#include "settings_store.h"
#include "updater.h"
#include "web_assets.h"  // generated from src/web/ by tools/web_assets.py

static const size_t MAX_BODY = 16384;  // a full settings JSON is ~5 KB, an event up to ~8 KB, karaoke ~3 KB

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

static void sendConflict() { server.send(409, "text/plain", "inzwischen auf einem anderen Gerät geändert"); }
static void sendSaveFailed() {
  server.send(500, "text/plain", "Speichern im Flash fehlgeschlagen (gilt bis zum Neustart)");
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
  // without ?rev= (a page from older firmware, or "save anyway") it just saves
  if (server.hasArg("rev") && server.arg("rev") != settingsRev()) {
    sendConflict();
    return;
  }
  JsonDocument doc(&spiRam);
  if (!readBody(doc)) return;
  Settings& s = settingsScratch();
  if (!settingsFromJson(doc.as<JsonVariantConst>(), s)) {
    server.send(400, "text/plain", "ungültige Daten");
    return;
  }
  if (!settingsSave(s)) {
    sendSaveFailed();
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

// The scenes and brightness from the event page, with ?rev= like
// handleSave(). Whether it is on stays as it is (/api/event/on).
static void handleEventSave() {
  if (server.hasArg("rev") && server.arg("rev") != eventRev()) {
    sendConflict();
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
  e.karaoke = eventNet().karaoke;
  if (!eventSave(e)) {
    sendSaveFailed();
    return;
  }
  sendEvent();
}

// {"on": true|false, "karaoke": true|false}: switches the event mode, with
// the karaoke list instead of the scenes; those stay as they are.
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
  e.karaoke = e.on && (doc["karaoke"] | false);
  if (!eventSave(e)) {
    sendSaveFailed();
    return;
  }
  sendEvent();
}

// The board's pixels for the previews on the page, drawn by the same code
// as on the panel. A canvas in PSRAM (large mallocs go there), cleared;
// nullptr (and 500 sent) if there is no memory.
static const int PREVIEW_N = PANEL_RES_X * PANEL_CHAIN * PANEL_RES_Y;
static GFXcanvas16* previewCanvas(U8G2_FOR_ADAFRUIT_GFX*& u8out) {
  static GFXcanvas16* canvas = nullptr;  // 24 KB
  static U8G2_FOR_ADAFRUIT_GFX u8;
  if (!canvas) {
    canvas = new GFXcanvas16(PANEL_RES_X * PANEL_CHAIN, PANEL_RES_Y);
    if (!canvas->getBuffer()) {
      delete canvas;
      canvas = nullptr;
      server.send(500, "text/plain", "out of memory");
      return nullptr;
    }
    u8.begin(*canvas);
  }
  canvas->fillScreen(0);
  u8out = &u8;
  return canvas;
}

// Sends the canvas as 192 x 64 RGB565 pixels row by row, run-length coded
// as pairs of uint16 (count, colour), little-endian. The page shows that,
// so its preview is exact without knowing the fonts; as an animation it
// asks every few hundred ms, the coding keeps that small (~2-6 KB).
static void sendPreview(GFXcanvas16& canvas) {
  static uint16_t* runs = nullptr;  // worst case a run per pixel
  if (!runs) runs = (uint16_t*)heap_caps_malloc(PREVIEW_N * 4, MALLOC_CAP_SPIRAM);
  if (!runs) {
    server.send(500, "text/plain", "out of memory");
    return;
  }
  const uint16_t* px = canvas.getBuffer();
  size_t n = 0;
  for (int i = 0; i < PREVIEW_N;) {
    int j = i + 1;
    while (j < PREVIEW_N && px[j] == px[i]) j++;
    runs[n++] = j - i;
    runs[n++] = px[i];
    i = j;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)runs, n * 2);
}

// One scene (JSON as in the event) drawn as the board would show it ?ms=
// after it came up (sendPreview()).
static void handleEventPreview() {
  JsonDocument doc(&spiRam);
  if (!readBody(doc)) return;
  Scene sc;
  if (!sceneFromJson(doc.as<JsonVariantConst>(), sc)) {
    server.send(400, "text/plain", "ungültige Daten");
    return;
  }
  U8G2_FOR_ADAFRUIT_GFX* u8;
  GFXcanvas16* canvas = previewCanvas(u8);
  if (!canvas) return;
  renderScene(*canvas, *u8, sc, strtoul(server.arg("ms").c_str(), nullptr, 10));
  sendPreview(*canvas);
}

// One scene (JSON as in the event) shown on the board for EVENT_TEST_S seconds.
static void handleEventTest() {
  JsonDocument doc(&spiRam);
  if (!readBody(doc)) return;
  Scene sc;
  if (!sceneFromJson(doc.as<JsonVariantConst>(), sc)) {
    server.send(400, "text/plain", "ungültige Daten");
    return;
  }
  eventTest(sc);
  server.send(200, "text/plain", "ok");
}

static void handleEventTestStop() {
  eventTestStop();
  server.send(200, "text/plain", "ok");
}

// ---- Karaoke list (src/karaoke.h) ----------------------------------------------

static String karaokeRev() { return String(bootTag, HEX) + "." + String(karaokeVersion()); }

static void sendKaraoke() {
  JsonDocument doc(&spiRam);
  karaokeToJson(karaokeNet(), doc.to<JsonObject>());
  doc["rev"] = karaokeRev();
  doc["max"] = MAX_KARAOKE;
  sendJson(doc);
}

// The whole list, with ?rev= like handleSave(). The page saves after every
// change and on 409 does it again on the newer list.
static void handleKaraokeSave() {
  if (server.hasArg("rev") && server.arg("rev") != karaokeRev()) {
    sendConflict();
    return;
  }
  JsonDocument doc(&spiRam);
  if (!readBody(doc)) return;
  Karaoke& k = karaokeScratch();
  if (!karaokeFromJson(doc.as<JsonVariantConst>(), k)) {
    server.send(400, "text/plain", "ungültige Daten");
    return;
  }
  if (!karaokeSave(k)) {
    sendSaveFailed();
    return;
  }
  sendKaraoke();
}

// The stored list as the board shows it ?ms= on (sendPreview()).
static void handleKaraokePreview() {
  U8G2_FOR_ADAFRUIT_GFX* u8;
  GFXcanvas16* canvas = previewCanvas(u8);
  if (!canvas) return;
  renderKaraoke(*canvas, *u8, karaokeNet(), strtoul(server.arg("ms").c_str(), nullptr, 10));
  sendPreview(*canvas);
}

// ---- Tram race (src/race.h) --------------------------------------------------

static void handleRace() {
  raceStart(esp_random());
  server.send(200, "text/plain", "ok");
}

static void handleRaceStop() {
  raceStop();
  server.send(200, "text/plain", "ok");
}

// ---- Pixel images (src/image_store.h) -------------------------------------------

// The list of images and the space left; `id`: the one just stored.
static void sendImages(int id = -1) {
  JsonDocument doc(&spiRam);
  JsonArray list = doc["images"].to<JsonArray>();
  for (int i = 0; i < MAX_IMAGES; i++) {
    ImageInfo info = imageInfo(i);
    if (!info.used) continue;
    JsonObject o = list.add<JsonObject>();
    o["id"] = i;
    o["name"] = (const char*)info.name;
    o["frames"] = info.frames;
    o["delayMs"] = info.delayMs;
  }
  doc["freeBytes"] = imageFreeBytes();
  doc["totalBytes"] = imageTotalBytes();
  doc["frameBytes"] = IMAGE_FRAME_BYTES;
  doc["maxFrames"] = MAX_IMAGE_FRAMES;
  doc["maxImages"] = MAX_IMAGES;
  if (id >= 0) doc["id"] = id;
  sendJson(doc);
}

// ?id= (&frame=: only that one): the frames as stored, RGB565 little-endian
static void handleImageGet() {
  int id = server.arg("id").toInt();
  ImageInfo info = imageInfo(id);
  const uint16_t* px = imagePixels(id);
  if (!info.used || !px) {
    server.send(404, "text/plain", "kein Bild");
    return;
  }
  int from = 0, n = info.frames;
  if (server.hasArg("frame")) {
    from = server.arg("frame").toInt();
    n = 1;
    if (from < 0 || from >= info.frames) {
      server.send(404, "text/plain", "kein solches Einzelbild");
      return;
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)(px + (size_t)from * IMAGE_W * IMAGE_H),
                (size_t)n * IMAGE_FRAME_BYTES);
}

// Upload as a multipart file "px" (the frames, as handleImageGet sends them)
// with ?id= (-1: a new one) &frames= &delay= &name=. The pieces go straight
// into PSRAM: a String of the body would take internal RAM and stops at the
// first 0 byte.
static uint16_t* upBuf = nullptr;
static size_t upLen = 0, upCap = 0;
static bool upFailed = false;

static void handleImageUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    heap_caps_free(upBuf);
    long frames = server.arg("frames").toInt();
    upCap = frames >= 1 && frames <= MAX_IMAGE_FRAMES ? (size_t)frames * IMAGE_FRAME_BYTES : 0;
    upBuf = upCap ? (uint16_t*)heap_caps_malloc(upCap, MALLOC_CAP_SPIRAM) : nullptr;
    upLen = 0;
    upFailed = !upBuf;
  } else if (u.status == UPLOAD_FILE_WRITE && !upFailed) {
    if (upLen + u.currentSize > upCap) upFailed = true;
    else memcpy((uint8_t*)upBuf + upLen, u.buf, u.currentSize), upLen += u.currentSize;
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    upFailed = true;
  }
}

static void handleImageSave() {
  uint16_t* px = upBuf;
  upBuf = nullptr;
  if (upFailed || !px || upLen != upCap) {
    heap_caps_free(px);
    server.send(400, "text/plain", "Bild unvollständig oder zu groß");
    return;
  }
  int id = server.hasArg("id") ? server.arg("id").toInt() : -1;
  if (id < 0)
    for (int i = 0; i < MAX_IMAGES && id < 0; i++)
      if (!imageInfo(i).used) id = i;
  if (id < 0 || id >= MAX_IMAGES) {
    heap_caps_free(px);
    server.send(400, "text/plain", "alle Plätze für Bilder belegt, bitte erst eines löschen");
    return;
  }
  char name[IMAGE_NAME_LEN];
  sanitizeUtf8(server.arg("name").c_str(), name, sizeof(name));
  long delay = server.arg("delay").toInt();
  const char* err;
  if (!imageStore(id, name, (uint8_t)(upCap / IMAGE_FRAME_BYTES), (uint16_t)(delay < 50 ? 50 : delay > 5000 ? 5000 : delay),
                  px, err)) {
    server.send(500, "text/plain", err);
    return;
  }
  sendImages(id);
}

// Also takes it out of the scenes that show it, so a new image that gets
// its number later doesn't turn up there.
static void handleImageDelete() {
  int id = server.arg("id").toInt();
  imageDelete(id);
  Event& e = eventScratch();
  e = eventNet();
  bool used = false;
  for (int i = 0; i < e.nScenes; i++)
    if (e.scenes[i].img == id) e.scenes[i].img = -1, used = true;
  if (used) eventSave(e);
  sendImages();
}

static void handleStatus() {
  time_t now = time(nullptr);
  JsonDocument doc(&spiRam);
  doc["build"] = firmwareBuild();
  doc["uptimeS"] = millis() / 1000;
  doc["dataAgeS"] = lastFetch && timeValid() ? (long)(now - lastFetch) : -1;
  doc["rev"] = settingsRev();
  doc["event"] = eventNet().on && !eventNet().karaoke;
  doc["karaoke"] = eventNet().on && eventNet().karaoke;
  doc["karaokeRev"] = karaokeRev();
  doc["eventRev"] = eventRev();
  doc["race"] = raceRunning();
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

// A page from src/web/, stored gzipped; every browser unpacks that itself.
static void sendPage(const uint8_t* gz, size_t len) {
  server.sendHeader("Cache-Control", "no-store");  // always the page of the running firmware
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (PGM_P)gz, len);
}

void webBegin() {
  if (started) return;
  bootTag = esp_random();  // WiFi is up, so this is truly random
  server.on("/", HTTP_GET, [] { sendPage(WEB_PAGE_GZ, sizeof(WEB_PAGE_GZ)); });
  for (const char* path : {"/favicon.png", "/favicon.ico", "/apple-touch-icon.png", "/apple-touch-icon-precomposed.png"}) {
    server.on(path, HTTP_GET, sendIcon);
  }
  server.on("/api/settings", HTTP_GET, sendSettings);
  server.on("/api/settings", HTTP_POST, handleSave);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/event", HTTP_GET, [] {  // the old event page, now a tab
    server.sendHeader("Location", "/#event");
    server.send(302, "text/plain", "");
  });
  server.on("/api/event", HTTP_GET, sendEvent);
  server.on("/api/event", HTTP_POST, handleEventSave);
  server.on("/api/event/on", HTTP_POST, handleEventOn);
  server.on("/api/event/preview", HTTP_POST, handleEventPreview);
  server.on("/api/event/test", HTTP_POST, handleEventTest);
  server.on("/api/event/test/stop", HTTP_POST, handleEventTestStop);
  server.on("/api/karaoke", HTTP_GET, sendKaraoke);
  server.on("/api/karaoke", HTTP_POST, handleKaraokeSave);
  server.on("/api/karaoke/preview", HTTP_GET, handleKaraokePreview);
  server.on("/api/race", HTTP_POST, handleRace);
  server.on("/api/race/stop", HTTP_POST, handleRaceStop);
  server.on("/api/images", HTTP_GET, [] { sendImages(); });
  server.on("/api/image", HTTP_GET, handleImageGet);
  server.on("/api/image", HTTP_POST, handleImageSave, handleImageUpload);
  server.on("/api/image/delete", HTTP_POST, handleImageDelete);
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
