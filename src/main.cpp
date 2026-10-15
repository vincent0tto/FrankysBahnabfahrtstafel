// Frankys Bahnabfahrtstafel
// rnv-style departure board for an ESP32-S3 + 3x 64x64 HUB75 panels.
// Data: Verkehrsverbund Rhein-Neckar (VRN), Datenlizenz Deutschland – Namensnennung.
//
// Core 0: netTask   – WiFi, NTP, OTA, settings web page, firmware update
//                     check after start, API polling
// Core 1: loop()    – renders ~30 fps from the last good data

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "display.h"
#include "log.h"
#include "net.h"
#include "settings_store.h"
#include "updater.h"
#include "vrn_client.h"
#include "web.h"

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

    // once per start, before the first fetch (the daily reboot falls into
    // the off time, so the update usually happens while the panel is dark)
    if (online && timeValid() && !updateChecked) {
      updateChecked = true;
      updateCheckAndInstall();  // restarts if it installed one
    }

    // Off time: no API calls; WAKE_FETCH_LEAD_S before its end they start
    // again so the panel comes on with fresh data
    time_t now = time(nullptr);
    const Settings& s = settingsNet();
    bool sleeping = timeValid() && isOffTime(s, minuteOfDay(now)) &&
                    isOffTime(s, minuteOfDay(now + WAKE_FETCH_LEAD_S));

    if (online && timeValid() && !sleeping && (int32_t)(millis() - nextFetchMs) >= 0) {
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

  esp_task_wdt_reset();
  if (boardVersion != localVersion && xSemaphoreTake(boardMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    local = sharedBoard;
    localVersion = boardVersion;
    xSemaphoreGive(boardMutex);
  }
  settingsFetch(settings, settingsVersion);

  DisplayStatus st;
  st.wifi = WiFi.status() == WL_CONNECTED;
  st.timeValid = timeValid();
  st.updating = updateRunning();
  bool dark = displayRender(local, settings, st, time(nullptr), millis());
  delay(dark ? 500 : 33);  // off time: nothing to draw
}
