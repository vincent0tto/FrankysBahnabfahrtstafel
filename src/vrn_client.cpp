#include "vrn_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>

#include "certs.h"
#include "config.h"
#include "parse.h"

// Keep the (filtered) JSON document in PSRAM to spare internal RAM for
// WiFi/TLS and the LED DMA buffers.
struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void deallocate(void* p) override { heap_caps_free(p); }
  void* reallocate(void* p, size_t n) override {
    return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
};
static SpiRamAllocator spiRam;

const char* fetchResultName(FetchResult r) {
  switch (r) {
    case FetchResult::Ok: return "ok";
    case FetchResult::NetworkError: return "network error";
    case FetchResult::HttpError: return "http error";
    case FetchResult::ParseError: return "parse error";
  }
  return "?";
}

static FetchResult fetchFrom(const char* baseUrl, Board& out) {
  String url = String(baseUrl) +
               "?outputFormat=rapidJSON&type_dm=any&mode=direct&useRealtime=1&depType=stopEvents"
               "&name_dm=" STOP_ID "&limit=" + String(API_LIMIT);

  WiFiClientSecure client;
  client.setCACert(VRN_ROOT_CA);
  client.setHandshakeTimeout(HTTP_TIMEOUT_MS / 1000);

  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding -> stream straight into the parser
  http.setReuse(false);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setUserAgent("FrankysBahnabfahrtstafel/1.0 (ESP32)");

  if (!http.begin(client, url)) return FetchResult::NetworkError;
  int code = http.GET();
  if (code <= 0) {
    char err[96] = "";
    client.lastError(err, sizeof(err));
    Serial.printf("[vrn] %s: %s (tls: %s)\n", baseUrl, http.errorToString(code).c_str(), err);
    http.end();
    return FetchResult::NetworkError;
  }
  if (code != HTTP_CODE_OK) {
    Serial.printf("[vrn] %s: HTTP %d\n", baseUrl, code);
    http.end();
    return FetchResult::HttpError;
  }

  JsonDocument filter;
  buildFilter(filter);
  JsonDocument doc(&spiRam);
  DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  // The server closes the socket right after the body; stop TLS ourselves so
  // http.end() doesn't try to drain a dead connection (logs mbedTLS -76).
  client.stop();
  http.end();
  if (err) {
    Serial.printf("[vrn] %s: JSON %s\n", baseUrl, err.c_str());
    return FetchResult::ParseError;
  }

  ParseOptions opt;
  opt.filterLines = FILTER_LINES;
  opt.filterPlatforms = FILTER_PLATFORMS;
  opt.abbreviations = DEST_ABBREVIATIONS;
  opt.platformDirections = PLATFORM_DIRECTIONS;
  opt.platformLabels = PLATFORM_LABELS;
  static Board tmp;  // only used from the network task
  time_t now = time(nullptr);
  if (!parseDepartures(doc.as<JsonVariantConst>(), now, opt, tmp)) {
    Serial.printf("[vrn] %s: response without stopEvents\n", baseUrl);
    return FetchResult::ParseError;
  }
  tmp.fetchedAt = now;
  out = tmp;
  return FetchResult::Ok;
}

FetchResult fetchBoard(Board& out) {
  FetchResult r = fetchFrom(API_URL_PRIMARY, out);
  if (r == FetchResult::Ok) return r;
  esp_task_wdt_reset();  // each attempt may take up to ~3x HTTP_TIMEOUT_MS
  return fetchFrom(API_URL_FALLBACK, out);
}
