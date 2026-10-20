#include "settings_store.h"

#include <Arduino.h>
#include <Preferences.h>

#include <new>

#include "json_file.h"
#include "log.h"
#include "psram_alloc.h"

static const char* const SETTINGS_FILE = "/settings.json";
// Older firmware kept them in NVS; taken over from there once.
static const char* const NVS_NAMESPACE = "tafel";
static const char* const NVS_KEY = "settings";

static Settings current;  // written by the network task only, under `mutex`
static volatile uint32_t currentVersion = 1;
static SemaphoreHandle_t mutex;

Settings& settingsScratch() {
  static Settings* s = new (heap_caps_malloc(sizeof(Settings), MALLOC_CAP_SPIRAM)) Settings;
  return *s;
}

// The settings as older firmware stored them in NVS, into `doc`; n = 0 if
// there are none.
static DeserializationError readNvs(JsonDocument& doc, size_t& n) {
  n = 0;
  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return DeserializationError::EmptyInput;  // never stored
  n = prefs.getBytesLength(NVS_KEY);
  char* buf = n ? (char*)heap_caps_malloc(n, MALLOC_CAP_SPIRAM) : nullptr;
  DeserializationError err = DeserializationError::EmptyInput;
  if (buf && prefs.getBytes(NVS_KEY, buf, n) == n) err = deserializeJson(doc, buf, n);
  heap_caps_free(buf);
  prefs.end();
  return err;
}

static void removeNvs() {
  Preferences prefs;
  if (prefs.begin(NVS_NAMESPACE, false)) {
    prefs.remove(NVS_KEY);
    prefs.end();
  }
}

// Writes `s` to the file; `n`: its size.
static bool store(const Settings& s, size_t& n) {
  JsonDocument doc(&spiRam);
  settingsToJson(s, doc.to<JsonObject>(), false);
  return jsonFileWrite(SETTINGS_FILE, doc, n);
}

void settingsBegin() {
  mutex = xSemaphoreCreateMutex();
  settingsDefaults(current);
  bool fs = jsonFsBegin();
  if (!fs) Log.println("[settings] file system not available, settings can't be stored");
  JsonDocument doc(&spiRam);
  size_t n;
  DeserializationError err = jsonFileRead(SETTINGS_FILE, doc, n);
  bool fromNvs = !n;
  if (fromNvs) {
    doc.clear();
    err = readNvs(doc, n);
  }
  if (!n) {
    Log.println("[settings] none stored, using defaults");
    return;
  }
  Settings& loaded = settingsScratch();
  if (err || !settingsFromJson(doc.as<JsonVariantConst>(), loaded)) {
    Log.printf("[settings] stored data broken (%s), using defaults\n", err.c_str());
    return;
  }
  current = loaded;
  Log.printf("[settings] loaded (%u bytes)%s\n", (unsigned)n, fromNvs ? " from NVS" : "");
  if (fromNvs && fs) {
    bool ok = store(current, n);
    if (ok) removeNvs();
    Log.printf("[settings] moved to the file system: %s\n", ok ? "ok" : "FAILED, NVS kept");
  }
}

const Settings& settingsNet() { return current; }

uint32_t settingsVersion() { return currentVersion; }

bool settingsSave(const Settings& s) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  current = s;
  currentVersion++;
  xSemaphoreGive(mutex);

  size_t n;
  bool ok = store(s, n);
  Log.printf("[settings] %s (%u bytes)\n", ok ? "saved" : "saving FAILED", (unsigned)n);
  return ok;
}

bool settingsFetch(Settings& out, uint32_t& version) {
  if (version == currentVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = current;
  version = currentVersion;
  xSemaphoreGive(mutex);
  return true;
}
