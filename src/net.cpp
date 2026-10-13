#include "net.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "config.h"
#include "secrets.h"
#include "web.h"

static uint32_t lastConnectedMs = 0;
static uint32_t lastKickMs = 0;
static bool wasConnected = false;
static bool otaStarted = false;

static void startOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([] { Serial.println("[ota] start"); });
  // handle() blocks for the whole upload; keep the watchdog quiet meanwhile
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { esp_task_wdt_reset(); });
  ArduinoOTA.onEnd([] { Serial.println("[ota] done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[ota] error %u\n", e); });
  ArduinoOTA.begin();
  otaStarted = true;
}

void netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);  // modem sleep causes latency and occasional drops
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
  lastConnectedMs = lastKickMs = millis();
}

bool netMaintain() {
  uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      Serial.printf("[wifi] connected, ip %s, rssi %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      wasConnected = true;
      if (!otaStarted) startOta();
      webBegin();
    }
    lastConnectedMs = now;
    ArduinoOTA.handle();
    webLoop();
    return true;
  }

  if (wasConnected) {
    Serial.println("[wifi] connection lost");
    wasConnected = false;
  }
  if (now - lastConnectedMs > WIFI_REBOOT_S * 1000UL) {
    Serial.println("[wifi] offline too long, rebooting");
    delay(100);
    ESP.restart();
  }
  if (now - lastKickMs > WIFI_RECONNECT_S * 1000UL) {
    Serial.println("[wifi] reconnecting");
    lastKickMs = now;
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
  return false;
}

bool timeValid() { return time(nullptr) > 1700000000; }
