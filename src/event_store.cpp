#include "event_store.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <new>

#include "log.h"
#include "psram_alloc.h"

static const char* const EVENT_FILE = "/event.json";
static const char* const EVENT_TMP = "/event.tmp";

static Event current;  // written by the network task only, under `mutex`
static volatile uint32_t currentVersion = 1;
static Scene testScene;  // same
static volatile uint32_t testVersion = 0;
static SemaphoreHandle_t mutex;
static bool mounted = false;

Event& eventScratch() {
  static Event* e = new (heap_caps_malloc(sizeof(Event), MALLOC_CAP_SPIRAM)) Event;
  return *e;
}

void eventBegin() {
  mutex = xSemaphoreCreateMutex();
  eventDefaults(current);
  mounted = LittleFS.begin(true);  // the partition is labelled "spiffs"
  if (!mounted) {
    Log.println("[event] file system not available, event can't be stored");
    return;
  }
  Log.printf("[event] file system %u of %u KB used\n", (unsigned)(LittleFS.usedBytes() / 1024),
             (unsigned)(LittleFS.totalBytes() / 1024));
  File f = LittleFS.open(EVENT_FILE, "r");
  if (!f) {
    Log.println("[event] none stored, using defaults");
    return;
  }
  JsonDocument doc(&spiRam);
  DeserializationError err = deserializeJson(doc, f);
  size_t n = f.size();
  f.close();
  Event& loaded = eventScratch();
  if (!err && eventFromJson(doc.as<JsonVariantConst>(), loaded)) {
    current = loaded;
    Log.printf("[event] loaded (%u bytes), %s\n", (unsigned)n, current.on ? "on" : "off");
  } else {
    Log.printf("[event] stored data broken (%s), using defaults\n", err.c_str());
  }
}

const Event& eventNet() { return current; }

uint32_t eventVersion() { return currentVersion; }

bool eventSave(const Event& e) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  current = e;
  currentVersion++;
  xSemaphoreGive(mutex);

  // written next to the old one and renamed over it, so a power cut leaves
  // either the old or the new event
  bool ok = false;
  size_t n = 0;
  if (mounted) {
    JsonDocument doc(&spiRam);
    eventToJson(e, doc.to<JsonObject>());
    File f = LittleFS.open(EVENT_TMP, "w");
    if (f) {
      n = serializeJson(doc, f);
      ok = n == measureJson(doc);
      f.close();
      ok = ok && LittleFS.rename(EVENT_TMP, EVENT_FILE);
    }
  }
  Log.printf("[event] %s (%u bytes), %s\n", ok ? "saved" : "saving FAILED", (unsigned)n, e.on ? "on" : "off");
  return ok;
}

void eventTest(const Scene& sc) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  testScene = sc;
  testVersion++;
  xSemaphoreGive(mutex);
  Log.printf("[event] testing scene \"%s\" on the board\n", sc.name);
}

bool eventTestFetch(Scene& out, uint32_t& version) {
  if (version == testVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = testScene;
  version = testVersion;
  xSemaphoreGive(mutex);
  return true;
}

bool eventFetch(Event& out, uint32_t& version) {
  if (version == currentVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = current;
  version = currentVersion;
  xSemaphoreGive(mutex);
  return true;
}
