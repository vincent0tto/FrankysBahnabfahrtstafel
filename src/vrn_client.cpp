#include "vrn_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>

#include "config.h"
#include "log.h"
#include "parse.h"
#include "psram_alloc.h"

static FetchResult fetchFrom(const char* baseUrl, Board& out) {
  String url = String(baseUrl) +
               "?outputFormat=rapidJSON&type_dm=any&mode=direct&useRealtime=1&depType=stopEvents"
               "&name_dm=" STOP_ID "&limit=" + String(API_LIMIT);

  JsonDocument filter;
  buildFilter(filter);
  JsonDocument doc(&spiRam);
  FetchResult r = httpsGetJson(url.c_str(), filter, doc, "vrn");
  if (r != FetchResult::Ok) return r;

  ParseOptions opt;
  opt.filterLines = FILTER_LINES;
  opt.filterPlatforms = FILTER_PLATFORMS;
  opt.abbreviations = DEST_ABBREVIATIONS;
  opt.platformDirections = PLATFORM_DIRECTIONS;
  opt.platformLabels = PLATFORM_LABELS;
  static Board tmp;  // only used from the network task
  time_t now = time(nullptr);
  if (!parseDepartures(doc.as<JsonVariantConst>(), now, opt, tmp)) {
    Log.printf("[vrn] %s: response without stopEvents\n", baseUrl);
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
