// Frankys Bahnabfahrtstafel
// rnv-style departure board for an ESP32-S3 + 3x 64x64 HUB75 panels.
// Data: Verkehrsverbund Rhein-Neckar (VRN), Datenlizenz Deutschland – Namensnennung.
//
// Core 0: netTask   – WiFi, NTP, OTA, settings web page, firmware update
//                     check after start, API polling, the St.-Pauli-Ticker
// Core 1: loop()    – renders ~30 fps from the last good data, the event,
//                     the tram race, a goal, the bouncer or the last round

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "core/log.h"
#include "core/net.h"
#include "core/settings_store.h"
#include "core/updater.h"
#include "departures/vrn_client.h"
#include "display/display.h"
#include "event/event_store.h"
#include "event/image_store.h"
#include "match/goal_render.h"
#include "match/match_client.h"
#include "web/web.h"

static Board sharedBoard;            // guarded by boardMutex
static volatile uint32_t boardVersion = 0;
static SemaphoreHandle_t boardMutex;

static void rebootChecks() {
  size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (freeInternal < MIN_FREE_HEAP_BYTES) {
    Log.printf("[sys] low internal heap (%u), rebooting\n", (unsigned)freeInternal);
    delay(100);
    ESP.restart();
  }
#if DAILY_REBOOT_HOUR >= 0
  if (timeValid() && millis() > 2UL * 3600 * 1000) {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    if (lt.tm_hour == DAILY_REBOOT_HOUR && lt.tm_min == 0) {
      Log.println("[sys] daily reboot");
      delay(100);
      ESP.restart();
    }
  }
#endif
}

static void netTask(void*) {
  esp_task_wdt_add(nullptr);
  netBegin();

  static Board fresh;
  uint32_t nextFetchMs = 0;
  uint32_t failures = 0;
  bool updateChecked = false;

  for (;;) {
    esp_task_wdt_reset();
    bool online = netMaintain();

    // The bouncer ends when the off time begins, and with it the event, so
    // the board goes dark (the event first, or it would light up in between)
    time_t now = time(nullptr);
    const Settings& s = settingsNet();
    if (bouncerRunning() && timeValid() && isOffTime(s, minuteOfDay(now))) {
      if (eventNet().on) {
        Event& e = eventScratch();
        e = eventNet();
        e.on = e.karaoke = false;
        eventSave(e);
      }
      bouncerStop();
    }

    // Event mode: no departures needed (unless the bouncer shows them), and
    // no update that would restart the board in the middle of it or of the
    // bouncer (checked once both are off)
    bool busy = eventNet().on || bouncerRunning();
    bool departures = !eventNet().on || bouncerRunning();

    // once per start, before the first fetch (the daily reboot falls into
    // the off time, so the update usually happens while the panel is dark)
    if (online && timeValid() && !updateChecked && !busy) {
      updateChecked = true;
      updateCheckAndInstall();  // restarts if it installed one
    }

    // Off time: no API calls; WAKE_FETCH_LEAD_S before its end they start
    // again so the panel comes on with fresh data
    bool sleeping = timeValid() && isOffTime(s, minuteOfDay(now)) &&
                    isOffTime(s, minuteOfDay(now + WAKE_FETCH_LEAD_S));

    if (online && timeValid() && !sleeping && departures && (int32_t)(millis() - nextFetchMs) >= 0) {
      FetchResult r = fetchBoard(fresh);
      if (r == FetchResult::Ok) {
        xSemaphoreTake(boardMutex, portMAX_DELAY);
        sharedBoard = fresh;
        boardVersion++;
        xSemaphoreGive(boardMutex);
        webNoteFetch(fresh.fetchedAt);
        failures = 0;
        Log.printf("[vrn] %d departures, heap %u, psram %u\n", fresh.n,
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
      } else {
        failures++;
        Log.printf("[vrn] %s (#%u), next try in %us\n", fetchResultName(r), (unsigned)failures,
                   (unsigned)POLL_RETRY_S);
      }
      nextFetchMs = millis() + (r == FetchResult::Ok ? POLL_INTERVAL_S : POLL_RETRY_S) * 1000UL;
    }
    esp_task_wdt_reset();
    matchMaintain(online && timeValid() && !sleeping && departures && s.matchOn);

    rebootChecks();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  logBegin();
  delay(500);
  Log.printf("\n[sys] Frankys Bahnabfahrtstafel, reset reason %d\n", (int)esp_reset_reason());

  boardMutex = xSemaphoreCreateMutex();
  settingsBegin();
  eventBegin();
  imageBegin();
  matchBegin();
  esp_task_wdt_init(WDT_TIMEOUT_S, true);  // panic -> reboot on hang
  esp_task_wdt_add(nullptr);               // loop task

  if (!displayBegin()) {
    delay(5000);
    ESP.restart();
  }
  Log.printf("[sys] display ok, free internal heap %u\n",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}

void loop() {
  static Board local;
  static uint32_t localVersion = UINT32_MAX;
  static Settings settings;
  static uint32_t settingsVersion = 0;
  static Event event;
  static uint32_t evVersion = 0;
  static uint32_t eventStartMs = 0;
  static Karaoke karaoke;
  static uint32_t kVersion = 0;
  static Event test;  // a scene tested from the event page, EVENT_TEST_S long
  static uint32_t testVersion = 0;
  static uint32_t testStartMs = 0;
  static bool testing = false;
  static Race race;  // the tram race started on the web page
  static uint32_t raceVersion = 0;
  static uint32_t raceStartMs = 0;
  static bool racing = false;
  static Match match;  // St.-Pauli-Ticker
  static uint32_t matchVersion = 0;
  static uint32_t goalVersion = 0;
  static uint32_t goalStartMs = 0;
  static bool goal = false;
  static bool bouncer = false;
  static uint32_t bouncerVersion = 0;
  static uint32_t bouncerStartMs = 0;
  static bool lastRound = false;
  static uint32_t lastRoundVersion = 0;
  static uint32_t lastRoundStartMs = 0;
  static uint32_t lastRoundDurMs = 0;

  esp_task_wdt_reset();
  if (boardVersion != localVersion && xSemaphoreTake(boardMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    local = sharedBoard;
    localVersion = boardVersion;
    xSemaphoreGive(boardMutex);
  }
  settingsFetch(settings, settingsVersion);
  if (eventFetch(event, evVersion)) {  // switched on or saved: from the first scene
    eventStartMs = millis();
    testing = false;
  }
  karaokeFetch(karaoke, kVersion);
  if (eventTestFetch(test.scenes[0], testing, testVersion)) testStartMs = millis();
  if (testing && millis() - testStartMs >= EVENT_TEST_S * 1000UL) testing = false;
  if (testing) {
    test.on = true;
    test.brightness = event.brightness;
    test.nScenes = 1;
  }

  uint32_t raceSeed;
  uint8_t fiestaChance;
  if (raceFetch(raceSeed, fiestaChance, racing, raceVersion) && racing) {
    raceBegin(race, raceSeed, fiestaChance);
    raceStartMs = millis();
  }
  if (racing && millis() - raceStartMs >= raceTotalMs(race)) racing = false;

  if (bouncerFetch(bouncer, bouncerVersion) && bouncer) bouncerStartMs = millis();
  if (lastRoundFetch(lastRound, lastRoundDurMs, lastRoundVersion) && lastRound) lastRoundStartMs = millis();
  if (lastRound && millis() - lastRoundStartMs >= lastRoundDurMs) lastRound = false;

  // a goal interrupts the departures only, not the event
  matchFetch(match, matchVersion);
  if (goalFetch(goalVersion) && (!event.on || bouncer)) {
    goal = true;
    goalStartMs = millis();
  }
  if (goal && millis() - goalStartMs >= GOAL_ANIM_MS) goal = false;

  DisplayStatus st;
  st.wifi = WiFi.status() == WL_CONNECTED;
  st.timeValid = timeValid();
  st.updating = updateRunning();
  uint32_t ms = millis();
  const Race* r = racing ? &race : nullptr;
  uint32_t goalMs = goal ? ms - goalStartMs : NO_GOAL;
  // a scene tested from the event page shows even over the bouncer and the last round
  bool dark = testing ? displayRender(local, settings, test, karaoke, match, st, time(nullptr), ms, ms - testStartMs, r,
                                      ms - raceStartMs, goalMs, nullptr, 0, nullptr, 0)
                      : displayRender(local, settings, event, karaoke, match, st, time(nullptr), ms, ms - eventStartMs, r,
                                      ms - raceStartMs, goalMs, bouncer ? &settings.bouncer : nullptr,
                                      ms - bouncerStartMs, lastRound ? &settings.lastRound : nullptr,
                                      ms - lastRoundStartMs);
  // a fixed frame rate however long the frame took (off time: nothing to
  // draw); if one took longer than that, wait a tick anyway and go on from now
  static TickType_t lastWake = xTaskGetTickCount();
  if (xTaskDelayUntil(&lastWake, pdMS_TO_TICKS(dark ? 500 : 33)) == pdFALSE) {
    vTaskDelay(1);
    lastWake = xTaskGetTickCount();
  }
}
