// Host tests: pio test -e native
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "departures/parse.h"

static time_t iso(const char* s) {
  time_t t = 0;
  TEST_ASSERT_TRUE(parseIsoUtc(s, t));
  return t;
}

static std::string readFixture() {
  for (const char* p : {"test/fixtures/schriesheim.json", "../test/fixtures/schriesheim.json"}) {
    std::ifstream f(p);
    if (f) { std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
  }
  TEST_FAIL_MESSAGE("fixture not found");
  return "";
}

static bool parseFixture(time_t now, const ParseOptions& opt, Board& b) {
  static std::string json = readFixture();
  JsonDocument filter;
  buildFilter(filter);
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  TEST_ASSERT_FALSE_MESSAGE(err, err.c_str());
  return parseDepartures(doc.as<JsonVariantConst>(), now, opt, b);
}

void setUp() {}
void tearDown() {}

void test_iso() {
  TEST_ASSERT_EQUAL_INT64(0, iso("1970-01-01T00:00:00Z"));
  TEST_ASSERT_EQUAL_INT64(1791456000, iso("2026-10-08T10:40:00Z"));
  TEST_ASSERT_EQUAL_INT64(1791456000, iso("2026-10-08T12:40:00+02:00"));
  TEST_ASSERT_EQUAL_INT64(1709208000, iso("2024-02-29T12:00:00Z"));  // leap day
  TEST_ASSERT_EQUAL_INT64(1791456000, iso("2026-10-08T10:40Z"));      // no seconds (ESPN)
  time_t t;
  TEST_ASSERT_FALSE(parseIsoUtc("", t));
  TEST_ASSERT_FALSE(parseIsoUtc(nullptr, t));
  TEST_ASSERT_FALSE(parseIsoUtc("garbage", t));
}

void test_sanitize() {
  char out[64];
  sanitizeUtf8("Mückenloch Ölweg Straße Ä", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Mückenloch Ölweg Straße Ä", out);
  sanitizeUtf8("abcdef", out, 4);  // truncation
  TEST_ASSERT_EQUAL_STRING("abc", out);
  sanitizeUtf8("abä", out, 4);     // no room for both bytes of ä -> dropped
  TEST_ASSERT_EQUAL_STRING("ab", out);
  sanitizeUtf8("A – B\n„C“", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("A - B \"C\"", out);
  sanitizeUtf8("A\xF0\x9F\x9A\x8B" "B", out, sizeof(out));  // emoji -> ?
  TEST_ASSERT_EQUAL_STRING("A?B", out);
}

void test_shorten() {
  char out[64];
  const char* abbr = "Hauptbahnhof|Hbf;Alter OEG-Bahnhof|Alter OEG-Bahnhof;Bahnhof|Bf";
  shortenDestination("Weinheim, Alter OEG-Bahnhof", abbr, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Weinheim Alter OEG-Bahnhof", out);
  shortenDestination("Weinheim, Bahnhof", abbr, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Weinheim Bf", out);
  shortenDestination("Heidelberg, Hauptbahnhof", "Hauptbahnhof|Hbf;Bahnhof|Bf", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Heidelberg Hbf", out);
}

void test_countdown() {
  Departure d{};
  char out[16];
  time_t now = iso("2026-10-08T10:35:00Z");
  d.when = now + 30;           formatCountdown(d, now, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("sofort", out);
  d.when = now - 20;           formatCountdown(d, now, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("sofort", out);
  d.when = now + 7 * 60 + 59;  formatCountdown(d, now, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("7 min", out);
  d.when = now + 59 * 60;      formatCountdown(d, now, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("59 min", out);
  // like the app: 19:00 at 18:52:30 -> 8 min (was 7 with second-exact math)
  time_t t1852 = iso("2026-10-08T16:52:30Z");
  d.when = iso("2026-10-08T17:00:00Z"); formatCountdown(d, t1852, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("8 min", out);
  d.when = iso("2026-10-08T17:00:00Z"); formatCountdown(d, iso("2026-10-08T16:59:59Z"), out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("1 min", out);
  formatCountdown(d, iso("2026-10-08T17:00:45Z"), out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("sofort", out);
  TEST_ASSERT_FALSE(hasDeparted(d, iso("2026-10-08T17:00:59Z")));
  TEST_ASSERT_TRUE(hasDeparted(d, iso("2026-10-08T17:01:00Z")));
  d.when = iso("2026-10-08T11:40:00Z");  // 13:40 CEST
  formatCountdown(d, now, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("13:40", out);
  d.cancelled = true;          formatCountdown(d, now, out, sizeof(out)); TEST_ASSERT_EQUAL_STRING("fällt aus", out);
}

void test_csv() {
  TEST_ASSERT_TRUE(csvContains("5,628", "628"));
  TEST_ASSERT_TRUE(csvContains("5, 628 ", "628"));
  TEST_ASSERT_FALSE(csvContains("5,628", "62"));
  TEST_ASSERT_FALSE(csvContains("", "5"));
}

void test_fixture_sorted_and_realtime() {
  Board b;
  ParseOptions opt;
  opt.abbreviations = "Alter OEG-Bahnhof|Alter OEG-Bahnhof;Bahnhof|Bf";
  TEST_ASSERT_TRUE(parseFixture(iso("2026-10-08T10:35:00Z"), opt, b));
  TEST_ASSERT_EQUAL_INT(MAX_DEPARTURES, b.n);
  // RNV 5 planned 10:20 but estimated 10:44:54 -> sorted after the 10:40 trains
  TEST_ASSERT_EQUAL_STRING("5", b.deps[0].line);
  TEST_ASSERT_EQUAL_STRING("Heidelberg", b.deps[0].dest);
  TEST_ASSERT_EQUAL_INT64(iso("2026-10-08T10:40:00Z"), b.deps[0].when);
  TEST_ASSERT_EQUAL_STRING("Heidelberg - Mannheim", b.deps[2].dest);
  TEST_ASSERT_EQUAL_INT64(iso("2026-10-08T10:44:54Z"), b.deps[2].when);
  TEST_ASSERT_TRUE(b.deps[2].realtime);
  TEST_ASSERT_EQUAL_STRING("Weinheim Alter OEG-Bahnhof", b.deps[3].dest);
  for (int i = 1; i < b.n; i++) TEST_ASSERT_TRUE(b.deps[i - 1].when <= b.deps[i].when);
  // bus 628 at 11:00 has no estimate
  bool found = false;
  for (int i = 0; i < b.n; i++)
    if (!strcmp(b.deps[i].line, "628") && b.deps[i].when == iso("2026-10-08T11:00:00Z")) {
      found = true;
      TEST_ASSERT_FALSE(b.deps[i].realtime);
      TEST_ASSERT_EQUAL_UINT8(6, b.deps[i].cls);
    }
  TEST_ASSERT_TRUE(found);
}

void test_fixture_filters_and_past() {
  Board b;
  ParseOptions opt;
  opt.filterLines = "628";
  TEST_ASSERT_TRUE(parseFixture(iso("2026-10-08T10:35:00Z"), opt, b));
  TEST_ASSERT_EQUAL_INT(4, b.n);
  for (int i = 0; i < b.n; i++) TEST_ASSERT_EQUAL_STRING("628", b.deps[i].line);

  opt = ParseOptions();
  opt.filterPlatforms = "TrRiN";
  TEST_ASSERT_TRUE(parseFixture(iso("2026-10-08T10:35:00Z"), opt, b));
  for (int i = 0; i < b.n; i++) TEST_ASSERT_EQUAL_STRING("Weinheim", b.deps[i].dest);

  // later "now": everything before 11:15 is gone
  opt = ParseOptions();
  TEST_ASSERT_TRUE(parseFixture(iso("2026-10-08T11:15:00Z"), opt, b));
  for (int i = 0; i < b.n; i++) TEST_ASSERT_FALSE(hasDeparted(b.deps[i], iso("2026-10-08T11:15:00Z")));
}

void test_error_and_empty() {
  Board b;
  JsonDocument doc;
  deserializeJson(doc, "{\"systemMessages\":[{\"type\":\"error\"}]}");
  TEST_ASSERT_FALSE(parseDepartures(doc.as<JsonVariantConst>(), 0, ParseOptions(), b));
  deserializeJson(doc,
      "{\"stopEvents\":[],\"x\":1}");
  TEST_ASSERT_TRUE(parseDepartures(doc.as<JsonVariantConst>(), 0, ParseOptions(), b));
  TEST_ASSERT_EQUAL_INT(0, b.n);
}

void test_cancel_and_ticker() {
  Board b;
  JsonDocument doc;
  deserializeJson(doc, R"({"stopEvents":[
    {"departureTimePlanned":"2026-10-08T10:40:00Z","realtimeStatus":["TRIP_CANCELLED"],
     "transportation":{"number":"RNV 5","product":{"class":4},"destination":{"name":"Mückenloch"}},
     "infos":[{"id":"a","infoLinks":[{"urlText":"Störung Linie 5"}]}]},
    {"departureTimePlanned":"2026-10-08T10:41:00Z","isCancelled":true,
     "transportation":{"disassembledName":"628","destination":{"name":"X"}},
     "infos":[{"id":"a","infoLinks":[{"urlText":"Störung Linie 5"}]},{"id":"b","infoLinks":[{"urlText":"Umleitung"}]}]}
  ]})");
  TEST_ASSERT_TRUE(parseDepartures(doc.as<JsonVariantConst>(), iso("2026-10-08T10:30:00Z"), ParseOptions(), b));
  TEST_ASSERT_EQUAL_INT(2, b.n);
  TEST_ASSERT_EQUAL_STRING("5", b.deps[0].line);  // "RNV " prefix stripped
  TEST_ASSERT_TRUE(b.deps[0].cancelled);
  TEST_ASSERT_FALSE(b.deps[1].cancelled);  // stop-level isCancelled only
  TEST_ASSERT_EQUAL_STRING("Mückenloch", b.deps[0].dest);
  TEST_ASSERT_EQUAL_STRING("Störung Linie 5 +++ Umleitung", b.ticker);
}

// Real event from 2026-10-08 19:00 (Schriesheim -> Heidelberg): trip running,
// only this stop flagged DEPARTURE_CANCELLED -> must not show "fällt aus".
void test_stop_level_cancel_ignored() {
  Board b;
  JsonDocument filter, doc;
  buildFilter(filter);
  deserializeJson(doc, R"({"stopEvents":[{"isCancelled":true,"realtimeStatus":["EXTRA_STOPS","MONITORED"],
    "location":{"properties":{"realtimeStatus":["DEPARTURE_CANCELLED"],"platform":"TrRiS"}},
    "departureTimePlanned":"2026-10-08T17:00:00Z",
    "transportation":{"disassembledName":"5","product":{"class":4},"destination":{"name":"Heidelberg"}}}]})",
    DeserializationOption::Filter(filter));
  TEST_ASSERT_TRUE(parseDepartures(doc.as<JsonVariantConst>(), iso("2026-10-08T16:52:00Z"), ParseOptions(), b));
  TEST_ASSERT_EQUAL_INT(1, b.n);
  TEST_ASSERT_FALSE(b.deps[0].cancelled);
}

// rnv replaces a trip by an EXTRA_TRIP and only flags the original isCancelled
void test_replaced_by_extra_trip() {
  Board b;
  JsonDocument filter, doc;
  buildFilter(filter);
  deserializeJson(doc, R"({"stopEvents":[
    {"isCancelled":true,"realtimeStatus":["MONITORED"],"location":{"properties":{"platform":"TrRiN"}},
     "departureTimePlanned":"2026-10-09T16:40:00Z",
     "transportation":{"disassembledName":"5","product":{"class":4},"destination":{"name":"Weinheim"}}},
    {"isCancelled":true,"realtimeStatus":["EXTRA_STOPS","MONITORED"],"location":{"properties":{"platform":"TrRiS"}},
     "departureTimePlanned":"2026-10-09T16:40:00Z",
     "transportation":{"disassembledName":"5","product":{"class":4},"destination":{"name":"Heidelberg"}}},
    {"realtimeStatus":["EXTRA_TRIP","MONITORED"],"location":{"properties":{"platform":"TrRiN"}},
     "departureTimePlanned":"2026-10-09T16:40:00Z","departureTimeEstimated":"2026-10-09T16:40:00Z",
     "transportation":{"disassembledName":"5","product":{"class":4},"destination":{"name":"Mannheim - Heidelberg"}}}]})",
    DeserializationOption::Filter(filter));
  TEST_ASSERT_TRUE(parseDepartures(doc.as<JsonVariantConst>(), iso("2026-10-09T16:30:00Z"), ParseOptions(), b));
  TEST_ASSERT_EQUAL_INT(2, b.n);  // other platform stays
  TEST_ASSERT_EQUAL_STRING("Heidelberg", b.deps[0].dest);
  TEST_ASSERT_EQUAL_STRING("Mannheim - Heidelberg", b.deps[1].dest);
}

// VRN sends one 628 departure twice with different trip codes (2026-10-12)
void test_duplicate_trip_dropped() {
  Board b;
  JsonDocument filter, doc;
  buildFilter(filter);
  deserializeJson(doc, R"({"stopEvents":[
    {"location":{"properties":{"platform":"B"}},"departureTimePlanned":"2026-10-12T11:10:00Z",
     "transportation":{"disassembledName":"628","product":{"class":6},"destination":{"name":"Wilhelmsfeld, Autohalle"}}},
    {"realtimeStatus":["MONITORED"],"location":{"properties":{"platform":"B"}},
     "departureTimePlanned":"2026-10-12T11:10:00Z","departureTimeEstimated":"2026-10-12T11:12:00Z",
     "transportation":{"disassembledName":"628","product":{"class":6},"destination":{"name":"Wilhelmsfeld, Autohalle"}}},
    {"location":{"properties":{"platform":"B"}},"departureTimePlanned":"2026-10-12T11:10:00Z",
     "transportation":{"disassembledName":"628","product":{"class":6},"destination":{"name":"Wilhelmsfeld, Autohalle"}}},
    {"location":{"properties":{"platform":"B"}},"departureTimePlanned":"2026-10-12T11:10:00Z",
     "transportation":{"disassembledName":"628","product":{"class":6},"destination":{"name":"Altenbach, Röschbach"}}}]})",
    DeserializationOption::Filter(filter));
  TEST_ASSERT_TRUE(parseDepartures(doc.as<JsonVariantConst>(), iso("2026-10-12T11:05:00Z"), ParseOptions(), b));
  TEST_ASSERT_EQUAL_INT(2, b.n);  // other destination stays
  TEST_ASSERT_EQUAL_STRING("Altenbach Röschbach", b.deps[0].dest);
  TEST_ASSERT_EQUAL_STRING("Wilhelmsfeld Autohalle", b.deps[1].dest);
  TEST_ASSERT_TRUE(b.deps[1].realtime);
}

// rnv names the board's own town as destination of trips that go on
void test_own_town_destination() {
  Board b;
  JsonDocument filter, doc;
  buildFilter(filter);
  deserializeJson(doc, R"({"stopEvents":[
    {"location":{"properties":{"platform":"TrRiN"},"parent":{"parent":{"name":"Schriesheim"}}},
     "departureTimePlanned":"2026-10-08T17:00:00Z",
     "transportation":{"disassembledName":"5","destination":{"name":"Schriesheim"}}},
    {"location":{"properties":{"platform":"B"},"parent":{"parent":{"name":"Schriesheim"}}},
     "departureTimePlanned":"2026-10-08T17:01:00Z",
     "transportation":{"disassembledName":"628","destination":{"name":"Schriesheim"}}},
    {"location":{"properties":{"platform":"TrRiS"},"parent":{"parent":{"name":"Schriesheim"}}},
     "departureTimePlanned":"2026-10-08T17:02:00Z",
     "transportation":{"disassembledName":"5","destination":{"name":"Heidelberg, Hauptbahnhof"}}}]})",
    DeserializationOption::Filter(filter));
  ParseOptions opt;
  opt.platformDirections = "TrRiN|Weinheim;TrRiS|Heidelberg";
  opt.platformLabels = "TrRiN|1;TrRiS|2";
  TEST_ASSERT_TRUE(parseDepartures(doc.as<JsonVariantConst>(), iso("2026-10-08T16:52:00Z"), opt, b));
  TEST_ASSERT_EQUAL_INT(3, b.n);
  TEST_ASSERT_EQUAL_STRING("1", b.deps[0].platformLabel);
  TEST_ASSERT_EQUAL_STRING("B", b.deps[1].platformLabel);  // no label -> code as is
  TEST_ASSERT_EQUAL_STRING("2", b.deps[2].platformLabel);
  TEST_ASSERT_EQUAL_STRING("Weinheim", b.deps[0].dest);
  TEST_ASSERT_EQUAL_STRING("Schriesheim", b.deps[1].dest);  // no rule for the platform
  TEST_ASSERT_EQUAL_STRING("Heidelberg Hauptbahnhof", b.deps[2].dest);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  UNITY_BEGIN();
  RUN_TEST(test_iso);
  RUN_TEST(test_sanitize);
  RUN_TEST(test_shorten);
  RUN_TEST(test_countdown);
  RUN_TEST(test_csv);
  RUN_TEST(test_fixture_sorted_and_realtime);
  RUN_TEST(test_fixture_filters_and_past);
  RUN_TEST(test_error_and_empty);
  RUN_TEST(test_cancel_and_ticker);
  RUN_TEST(test_stop_level_cancel_ignored);
  RUN_TEST(test_replaced_by_extra_trip);
  RUN_TEST(test_duplicate_trip_dropped);
  RUN_TEST(test_own_town_destination);
  return UNITY_END();
}
