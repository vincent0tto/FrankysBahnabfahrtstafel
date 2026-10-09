#pragma once
// Pure parsing/formatting helpers. No Arduino dependency so they can be
// unit-tested on the host (pio test -e native).
#include <ArduinoJson.h>
#include <stddef.h>
#include <time.h>
#include "departure.h"

struct ParseOptions {
  const char* filterLines = "";       // CSV, empty = all
  const char* filterPlatforms = "";   // CSV, empty = all
  const char* abbreviations = "";     // "from|to;from|to"
  // "platform|destination;...": used when a trip's destination is just the
  // board's own town (rnv sends e.g. "Schriesheim" for trips that go on).
  const char* platformDirections = "";
};

// Filter for deserializeJson so only the fields we need are kept in memory.
void buildFilter(JsonDocument& filter);

// Fills `out` (deps, n, ticker; not fetchedAt) from an EFA rapidJSON
// XML_DM_REQUEST response. Departures whose minute is over are dropped,
// the rest is sorted by effective time. Returns false if the response has no
// "stopEvents" array (= API error); an empty array is a valid result.
bool parseDepartures(JsonVariantConst root, time_t now, const ParseOptions& opt, Board& out);

// "2026-10-08T10:20:00Z" -> UTC epoch. Returns false on malformed input.
bool parseIsoUtc(const char* s, time_t& out);

// Keeps only characters the display fonts have (ASCII + Latin-1, e.g. umlauts,
// ß, é); typographic dashes/quotes -> ASCII, anything else -> '?'.
// Output is UTF-8, never splits a character, always NUL-terminates.
size_t sanitizeUtf8(const char* in, char* out, size_t outSize);

// Destination cleanup on UTF-8 input: ", " -> " ", apply abbreviations.
void shortenDestination(const char* in, const char* abbreviations, char* out, size_t outSize);

// Clock minutes from now until `when` (19:00 at 18:52:xx -> 8).
long minutesUntil(time_t when, time_t now);

// true once the departure minute is over.
bool hasDeparted(const Departure& d, time_t now);

// Right column text (UTF-8): "sofort", "7 min", "11:40", "fällt aus".
void formatCountdown(const Departure& d, time_t now, char* out, size_t outSize);

// true if `item` is one of the comma separated values in `csv`.
bool csvContains(const char* csv, const char* item);
