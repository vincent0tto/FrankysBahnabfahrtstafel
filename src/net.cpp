#include "net.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "config.h"
#include "credentials.h"
#include "log.h"
#include "web.h"

static uint32_t lastConnectedMs = 0;
static uint32_t lastKickMs = 0;
static bool wasConnected = false;
static bool otaStarted = false;
static bool otaActive = false;
static bool usingPrevious = false;  // trying the WiFi from before the last change

static void connect() {
  const char* ssid = credentialsSsid(usingPrevious);
  if (!ssid[0]) {
    Log.println("[wifi] no credentials, flash once with include/secrets.h");
    return;
  }
  Log.printf("[wifi] connecting to \"%s\"%s\n", ssid, usingPrevious ? " (previous)" : "");
  WiFi.begin(ssid, credentialsPass(usingPrevious));
}

static void startOta() {
  if (!credentialsOta()[0]) {
    Log.println("[ota] no password stored, OTA off");
    MDNS.begin(HOSTNAME);  // otherwise done by ArduinoOTA, the web page needs it
    otaStarted = true;
    return;
  }
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(credentialsOta());
  ArduinoOTA.onStart([] { Log.println("[ota] start"); });
  // handle() blocks for the whole upload; keep the watchdog quiet meanwhile
  ArduinoOTA.onProgress([](unsigned int, unsigned int) { esp_task_wdt_reset(); });
  ArduinoOTA.onEnd([] { Log.println("[ota] done, rebooting"); });
  ArduinoOTA.onError([](ota_error_t e) { Log.printf("[ota] error %u\n", e); });
  ArduinoOTA.begin();
  otaStarted = otaActive = true;
}

void netBegin() {
  credentialsBegin();
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(false);  // modem sleep causes latency and occasional drops
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  connect();
  configTzTime(TZ_INFO, NTP_SERVER1, NTP_SERVER2);
  lastConnectedMs = lastKickMs = millis();
}

bool netMaintain() {
  uint32_t now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      Log.printf("[wifi] connected, ip %s, rssi %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      wasConnected = true;
      if (usingPrevious) {
        credentialsSwap();
        usingPrevious = false;
      }
      if (!otaStarted) startOta();
      webBegin();
    }
    lastConnectedMs = now;
    if (otaActive) ArduinoOTA.handle();
    webLoop();
    return true;
  }

  if (wasConnected) {
    Log.println("[wifi] connection lost");
    wasConnected = false;
  }
  if (now - lastConnectedMs > WIFI_REBOOT_S * 1000UL) {
    Log.println("[wifi] offline too long, rebooting");
    delay(100);
    ESP.restart();
  }
  if (now - lastKickMs > WIFI_RECONNECT_S * 1000UL) {
    Log.println("[wifi] reconnecting");
    lastKickMs = now;
    WiFi.disconnect();
    // alternate with the previous WiFi, so a mistyped change doesn't lock the board out
    if (credentialsSsid(true)[0]) usingPrevious = !usingPrevious;
    connect();
  }
  return false;
}

bool timeValid() { return time(nullptr) > 1700000000; }
