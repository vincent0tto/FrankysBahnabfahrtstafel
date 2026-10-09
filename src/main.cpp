// Frankys Bahnabfahrtstafel
// rnv-style departure board for an ESP32-S3 + 3x 64x64 HUB75 panels.
// Data: Verkehrsverbund Rhein-Neckar (VRN), Datenlizenz Deutschland – Namensnennung.
//
// Core 0: netTask   – WiFi, NTP, OTA, API polling with backoff
// Core 1: loop()    – renders ~30 fps from the last good data

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "display.h"
#include "net.h"
#include "vrn_client.h"

static Board sharedBoard;            // guarded by boardMutex
static volatile uint32_t boardVersion = 0;
static SemaphoreHandle_t boardMutex;

static void rebootChecks() {
  size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  if (freeInternal < MIN_FREE_HEAP_BYTES) {
    Serial.printf("[sys] low internal heap (%u), rebooting\n", (unsigned)freeInternal);
    delay(100);
    ESP.restart();
  }
#if DAILY_REBOOT_HOUR >= 0
  if (timeValid() && millis() > 2UL * 3600 * 1000) {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    if (lt.tm_hour == DAILY_REBOOT_HOUR && lt.tm_min == 0) {
      Serial.println("[sys] daily reboot");
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
  uint32_t backoffS = POLL_INTERVAL_S;
  uint32_t failures = 0;

  for (;;) {
    esp_task_wdt_reset();
    bool online = netMaintain();

    if (online && timeValid() && (int32_t)(millis() - nextFetchMs) >= 0) {
      FetchResult r = fetchBoard(fresh);
      if (r == FetchResult::Ok) {
        xSemaphoreTake(boardMutex, portMAX_DELAY);
        sharedBoard = fresh;
        boardVersion++;
        xSemaphoreGive(boardMutex);
        failures = 0;
        backoffS = POLL_INTERVAL_S;
        Serial.printf("[vrn] %d departures, heap %u, psram %u\n", fresh.n,
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
      } else {
        failures++;
        backoffS = min<uint32_t>(backoffS * 2, POLL_BACKOFF_MAX_S);
        Serial.printf("[vrn] %s (#%u), next try in %us\n", fetchResultName(r), (unsigned)failures,
                      (unsigned)backoffS);
      }
      nextFetchMs = millis() + (r == FetchResult::Ok ? POLL_INTERVAL_S : backoffS) * 1000UL;
    }

    rebootChecks();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n[sys] Frankys Bahnabfahrtstafel, reset reason %d\n", (int)esp_reset_reason());

  boardMutex = xSemaphoreCreateMutex();
  esp_task_wdt_init(WDT_TIMEOUT_S, true);  // panic -> reboot on hang
  esp_task_wdt_add(nullptr);               // loop task

  if (!displayBegin()) {
    delay(5000);
    ESP.restart();
  }
  Serial.printf("[sys] display ok, free internal heap %u\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}

void loop() {
  static Board local;
  static uint32_t localVersion = UINT32_MAX;

  esp_task_wdt_reset();
  if (boardVersion != localVersion && xSemaphoreTake(boardMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    local = sharedBoard;
    localVersion = boardVersion;
    xSemaphoreGive(boardMutex);
  }

  DisplayStatus st;
  st.wifi = WiFi.status() == WL_CONNECTED;
  st.timeValid = timeValid();
  displayRender(local, st, time(nullptr), millis());
  delay(33);
}
