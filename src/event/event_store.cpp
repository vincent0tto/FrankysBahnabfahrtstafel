#include "event/event_store.h"

#include <Arduino.h>
#include <LittleFS.h>

#include <new>

#include "core/json_file.h"
#include "core/log.h"
#include "core/psram_alloc.h"
#include "race/race.h"

static const char* const EVENT_FILE = "/event.json";
static const char* const KARAOKE_FILE = "/karaoke.json";

static Event current;  // written by the network task only, under `mutex`
static volatile uint32_t currentVersion = 1;
static Karaoke karaoke;  // same
static volatile uint32_t karaokeVer = 1;
static Scene testScene;  // same
static bool testActive = false;
static volatile uint32_t testVersion = 0;
static uint32_t raceSeed = 0;  // same
static uint8_t raceFiestaPct = 0;
static bool raceActive = false;
static uint32_t raceEndMs = 0;  // millis() when it is over
static volatile uint32_t raceVersion = 0;
static bool bouncerActive = false;  // same
static volatile uint32_t bouncerVersion = 0;
static bool lastRoundActive = false;  // same
static uint32_t lastRoundDurMs = 0;
static uint32_t lastRoundEndMs = 0;  // millis() when it is over
static volatile uint32_t lastRoundVersion = 0;
static SemaphoreHandle_t mutex;

Event& eventScratch() {
  static Event* e = new (heap_caps_malloc(sizeof(Event), MALLOC_CAP_SPIRAM)) Event;
  return *e;
}

Karaoke& karaokeScratch() {
  static Karaoke* k = new (heap_caps_malloc(sizeof(Karaoke), MALLOC_CAP_SPIRAM)) Karaoke;
  return *k;
}

static void karaokeLoad() {
  JsonDocument doc(&spiRam);
  size_t n;
  DeserializationError err = jsonFileRead(KARAOKE_FILE, doc, n);
  if (!n) return;  // none yet: the defaults
  Karaoke& loaded = karaokeScratch();
  if (!err && karaokeFromJson(doc.as<JsonVariantConst>(), loaded)) {
    karaoke = loaded;
    Log.printf("[karaoke] loaded, %d entries\n", karaoke.n);
  } else {
    Log.printf("[karaoke] stored data broken (%s), using defaults\n", err.c_str());
  }
}

void eventBegin() {
  mutex = xSemaphoreCreateMutex();
  eventDefaults(current);
  karaokeDefaults(karaoke);
  if (!jsonFsBegin()) {
    Log.println("[event] file system not available, event can't be stored");
    return;
  }
  karaokeLoad();
  Log.printf("[event] file system %u of %u KB used\n", (unsigned)(LittleFS.usedBytes() / 1024),
             (unsigned)(LittleFS.totalBytes() / 1024));
  JsonDocument doc(&spiRam);
  size_t n;
  DeserializationError err = jsonFileRead(EVENT_FILE, doc, n);
  if (!n) {
    Log.println("[event] none stored, using defaults");
    return;
  }
  Event& loaded = eventScratch();
  if (!err && eventFromJson(doc.as<JsonVariantConst>(), loaded)) {
    current = loaded;
    Log.printf("[event] loaded (%u bytes), %s\n", (unsigned)n, current.on ? current.karaoke ? "karaoke" : "on" : "off");
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

  JsonDocument doc(&spiRam);
  eventToJson(e, doc.to<JsonObject>());
  size_t n;
  bool ok = jsonFileWrite(EVENT_FILE, doc, n);
  Log.printf("[event] %s (%u bytes), %s\n", ok ? "saved" : "saving FAILED", (unsigned)n,
             e.on ? e.karaoke ? "karaoke" : "on" : "off");
  return ok;
}

const Karaoke& karaokeNet() { return karaoke; }

uint32_t karaokeVersion() { return karaokeVer; }

bool karaokeSave(const Karaoke& k) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  karaoke = k;
  karaokeVer++;
  xSemaphoreGive(mutex);

  JsonDocument doc(&spiRam);
  karaokeToJson(k, doc.to<JsonObject>());
  size_t n;
  bool ok = jsonFileWrite(KARAOKE_FILE, doc, n);
  Log.printf("[karaoke] %s (%u bytes), %d entries\n", ok ? "saved" : "saving FAILED", (unsigned)n, k.n);
  return ok;
}

bool karaokeFetch(Karaoke& out, uint32_t& version) {
  if (version == karaokeVer || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = karaoke;
  version = karaokeVer;
  xSemaphoreGive(mutex);
  return true;
}

void eventTest(const Scene& sc) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  testScene = sc;
  testActive = true;
  testVersion++;
  xSemaphoreGive(mutex);
  Log.printf("[event] testing scene \"%s\" on the board\n", sc.name);
}

void eventTestStop() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  testActive = false;
  testVersion++;
  xSemaphoreGive(mutex);
  Log.println("[event] test stopped");
}

bool eventTestFetch(Scene& out, bool& active, uint32_t& version) {
  if (version == testVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  out = testScene;
  active = testActive;
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

const Race& raceStart(uint32_t seed, uint8_t fiestaChance) {
  static Race r;  // to know how long it takes and when the Mexican comes
  raceBegin(r, seed, fiestaChance);
  while (fiestaChance == 1 && !r.fiesta) raceBegin(r, raceRand(seed), fiestaChance);  // not one with a derailment
  xSemaphoreTake(mutex, portMAX_DELAY);
  raceSeed = seed;
  raceFiestaPct = fiestaChance;
  raceActive = true;
  raceEndMs = millis() + raceTotalMs(r);
  raceVersion++;
  xSemaphoreGive(mutex);
  Log.printf("[race] started, lane %d wins after %u ms%s\n", r.winner + 1, (unsigned)r.endMs,
             r.fiesta ? ", with fiesta" : "");
  return r;
}

void raceStop() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  raceActive = false;
  raceVersion++;
  xSemaphoreGive(mutex);
  Log.println("[race] stopped");
}

bool raceRunning() { return raceActive && (int32_t)(millis() - raceEndMs) < 0; }

bool raceFetch(uint32_t& seed, uint8_t& fiestaChance, bool& active, uint32_t& version) {
  if (version == raceVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  seed = raceSeed;
  fiestaChance = raceFiestaPct;
  active = raceActive;
  version = raceVersion;
  xSemaphoreGive(mutex);
  return true;
}

void bouncerStart() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bouncerActive = true;
  bouncerVersion++;
  xSemaphoreGive(mutex);
  Log.println("[bouncer] on");
}

void bouncerStop() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bouncerActive = false;
  bouncerVersion++;
  xSemaphoreGive(mutex);
  Log.println("[bouncer] off");
}

bool bouncerRunning() { return bouncerActive; }

bool bouncerFetch(bool& active, uint32_t& version) {
  if (version == bouncerVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  active = bouncerActive;
  version = bouncerVersion;
  xSemaphoreGive(mutex);
  return true;
}

void lastRoundStart(uint32_t durMs) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  lastRoundActive = true;
  lastRoundDurMs = durMs;
  lastRoundEndMs = millis() + durMs;
  lastRoundVersion++;
  xSemaphoreGive(mutex);
  Log.printf("[last round] on for %u s\n", (unsigned)(durMs / 1000));
}

void lastRoundStop() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  lastRoundActive = false;
  lastRoundVersion++;
  xSemaphoreGive(mutex);
  Log.println("[last round] off");
}

bool lastRoundRunning() { return lastRoundActive && (int32_t)(millis() - lastRoundEndMs) < 0; }

bool lastRoundFetch(bool& active, uint32_t& durMs, uint32_t& version) {
  if (version == lastRoundVersion || xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) != pdTRUE) return false;
  active = lastRoundActive;
  durMs = lastRoundDurMs;
  version = lastRoundVersion;
  xSemaphoreGive(mutex);
  return true;
}
