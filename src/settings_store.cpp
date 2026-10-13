#include "settings_store.h"

#include <Arduino.h>
#include <Preferences.h>

#include <new>

#include "psram_alloc.h"

static const char* const NVS_NAMESPACE = "tafel";
static const char* const NVS_KEY = "settings";

static Settings current;  // written by the network task only, under `mutex`
static volatile uint32_t currentVersion = 1;
static SemaphoreHandle_t mutex;

Settings& settingsScratch() {
  static Settings* s = new (heap_caps_malloc(sizeof(Settings), MALLOC_CAP_SPIRAM)) Settings;
  return *s;
}

void settingsBegin() {
  mutex = xSemaphoreCreateMutex();
  settingsDefaults(current);
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) {  // fails if nothing was ever stored
    Serial.println("[settings] none stored, using defaults");
    return;
  }
  size_t n = prefs.getBytesLength(NVS_KEY);
  char* buf = n ? (char*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM) : nullptr;
  if (buf && prefs.getBytes(NVS_KEY, buf, n) == n) {
    JsonDocument doc(&spiRam);
    DeserializationError err = deserializeJson(doc, buf, n);
    Settings& loaded = settingsScratch();
    if (!err && settingsFromJson(doc.as<JsonVariantConst>(), loaded)) {
      current = loaded;
      Serial.printf("[settings] loaded (%u bytes)\n", (unsigned)n);
    } else {
      Serial.printf("[settings] stored data broken (%s), using defaults\n", err.c_str());
    }
  }
  heap_caps_free(buf);
  prefs.end();
}

const Settings& settingsNet() { return current; }

bool settingsSave(const Settings& s) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  current = s;
  currentVersion++;
  xSemaphoreGive(mutex);

  JsonDocument doc(&spiRam);
  settingsToJson(s, doc.to<JsonObject>(), false);
  size_t n = measureJson(doc);
  char* buf = (char*)heap_caps_malloc(n + 1, MALLOC_CAP_SPIRAM);
  bool ok = false;
  Preferences prefs;
  if (buf && prefs.begin(NVS_NAMESPACE, false)) {
    serializeJson(doc, buf, n + 1);
    ok = prefs.putBytes(NVS_KEY, buf, n) == n;
    prefs.end();
  }
  heap_caps_free(buf);
  Serial.printf("[settings] %s (%u bytes)\n", ok ? "saved" : "saving FAILED", (unsigned)n);
  return ok;
}

bool settingsFetch(Settings& out, uint32_t& version) {
  if (version == currentVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = current;
  version = currentVersion;
  xSemaphoreGive(mutex);
  return true;
}
