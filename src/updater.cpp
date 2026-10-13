#include "updater.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

#include "certs.h"
#include "config.h"
#include "psram_alloc.h"
#include "secrets.h"

#ifndef FW_BUILD
#define FW_BUILD 0
#endif
#ifndef UPDATE_TOKEN
#define UPDATE_TOKEN ""  // older secrets.h without it: no updates
#endif

static const uint32_t UPDATE_TIMEOUT_MS = 15000;
static volatile bool running = false;

int firmwareBuild() { return FW_BUILD; }
bool updateRunning() { return running; }

static void setupClient(WiFiClientSecure& client) {
  client.setCACert(GITHUB_ROOT_CA);
  client.setHandshakeTimeout(UPDATE_TIMEOUT_MS / 1000);
  static_cast<Stream&>(client).setTimeout(UPDATE_TIMEOUT_MS);  // see vrn_client.cpp
}

static void setupHttp(HTTPClient& http) {
  http.useHTTP10(true);
  http.setReuse(false);
  http.setConnectTimeout(UPDATE_TIMEOUT_MS);
  http.setTimeout(UPDATE_TIMEOUT_MS);
  http.setUserAgent("FrankysBahnabfahrtstafel/1.0 (ESP32)");  // GitHub requires one
}

// Newest release: its build number and the API URL of its firmware asset.
static bool latestRelease(int& build, String& assetUrl) {
  WiFiClientSecure client;
  setupClient(client);
  HTTPClient http;
  setupHttp(http);
  if (!http.begin(client, "https://api.github.com/repos/" GITHUB_REPO "/releases/latest")) return false;
  http.addHeader("Authorization", "Bearer " UPDATE_TOKEN);
  http.addHeader("Accept", "application/vnd.github+json");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[update] releases/latest: %s\n", code > 0 ? String(code).c_str() : http.errorToString(code).c_str());
    http.end();
    return false;
  }
  JsonDocument filter;
  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["url"] = true;
  JsonDocument doc(&spiRam);
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  client.stop();
  http.end();
  if (err) {
    Serial.printf("[update] releases/latest: JSON %s\n", err.c_str());
    return false;
  }
  const char* tag = doc["tag_name"] | "";
  if (strncmp(tag, "build-", 6) != 0) {
    Serial.printf("[update] unexpected tag \"%s\"\n", tag);
    return false;
  }
  build = atoi(tag + 6);
  for (JsonObject a : doc["assets"].as<JsonArray>())
    if (strcmp(a["name"] | "", UPDATE_ASSET) == 0) assetUrl = a["url"].as<const char*>();
  if (assetUrl.isEmpty()) Serial.printf("[update] %s has no " UPDATE_ASSET "\n", tag);
  return !assetUrl.isEmpty();
}

// The asset API answers with a redirect to a short-lived download link that
// must be fetched without the token.
static bool downloadLink(const String& assetUrl, String& link) {
  WiFiClientSecure client;
  setupClient(client);
  HTTPClient http;
  setupHttp(http);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const char* keep[] = {"Location"};
  http.collectHeaders(keep, 1);
  if (!http.begin(client, assetUrl)) return false;
  http.addHeader("Authorization", "Bearer " UPDATE_TOKEN);
  http.addHeader("Accept", "application/octet-stream");
  int code = http.GET();
  link = http.header("Location");
  client.stop();
  http.end();
  if (link.isEmpty()) Serial.printf("[update] asset: HTTP %d without redirect\n", code);
  return !link.isEmpty();
}

static bool install(const String& link) {
  WiFiClientSecure client;
  setupClient(client);
  HTTPClient http;
  setupHttp(http);
  if (!http.begin(client, link)) return false;
  int code = http.GET();
  int len = http.getSize();
  if (code != HTTP_CODE_OK || len <= 0) {
    Serial.printf("[update] download: HTTP %d, %d bytes\n", code, len);
    http.end();
    return false;
  }
  if (!Update.begin(len)) {
    Serial.printf("[update] begin: %s\n", Update.errorString());
    http.end();
    return false;
  }
  Serial.printf("[update] writing %d bytes\n", len);
  WiFiClient* stream = http.getStreamPtr();
  static uint8_t buf[2048];
  int done = 0;
  uint32_t lastData = millis();
  while (done < len && millis() - lastData < UPDATE_TIMEOUT_MS) {
    esp_task_wdt_reset();
    size_t avail = stream->available();
    if (!avail) {
      delay(5);
      continue;
    }
    int n = stream->readBytes(buf, min(avail, sizeof(buf)));
    if (n <= 0 || Update.write(buf, n) != (size_t)n) break;
    done += n;
    lastData = millis();
  }
  client.stop();
  http.end();
  if (done != len) {
    Serial.printf("[update] download stopped at %d of %d bytes (%s)\n", done, len, Update.errorString());
    Update.abort();
    return false;
  }
  if (!Update.end(true)) {  // checks the image
    Serial.printf("[update] end: %s\n", Update.errorString());
    return false;
  }
  return true;
}

void updateCheckAndInstall() {
#if AUTO_UPDATE
  if (!UPDATE_TOKEN[0]) {
    Serial.println("[update] no UPDATE_TOKEN in secrets.h, skipped");
    return;
  }
  int build = 0;
  String assetUrl;
  if (!latestRelease(build, assetUrl)) return;
  if (build <= FW_BUILD) {
    Serial.printf("[update] build %d is up to date (newest: %d)\n", FW_BUILD, build);
    return;
  }
  Serial.printf("[update] build-%d > %d, installing\n", build, FW_BUILD);
  running = true;
  String link;
  esp_task_wdt_reset();
  if (downloadLink(assetUrl, link) && install(link)) {
    Serial.println("[update] done, restarting");
    delay(200);
    ESP.restart();
  }
  running = false;
#endif
}
