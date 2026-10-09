#pragma once
#include <stdint.h>
#include <time.h>

#define MAX_DEPARTURES 12
#define TICKER_LEN     512

// All text fields are sanitized UTF-8 (see sanitizeUtf8).
struct Departure {
  char    line[8];       // "5", "628"
  char    dest[64];      // shortened destination
  char    platform[12];  // "TrRiS", "B" (only used for filtering/logging)
  time_t  when;          // effective departure (estimated if known), UTC
  time_t  planned;       // timetable departure, UTC
  bool    realtime;      // departureTimeEstimated was present
  bool    cancelled;
  uint8_t cls;           // EFA product class: 4 = tram/Stadtbahn, 5/6 = bus
};

struct Board {
  Departure deps[MAX_DEPARTURES];
  int       n = 0;
  char      ticker[TICKER_LEN] = "";  // disruption messages
  time_t    fetchedAt = 0;            // 0 = never fetched successfully
};
