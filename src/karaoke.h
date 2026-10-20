#pragma once
// Karaoke list: who sings what, in order. The board shows a heading and the
// first KARAOKE_ROWS entries (src/karaoke_render.cpp); once the first is
// deleted the others move up. Each row on the board has its own colour,
// which stays with the row, not with the entry. Shown while the event is on
// with Event::karaoke set. No Arduino dependency, unit-tested on the host.
#include <ArduinoJson.h>
#include <stdint.h>

#define MAX_KARAOKE  30
#define KARAOKE_ROWS 4

struct KaraokeEntry {
  char name[24] = "";
  char song[48] = "";
};

struct Karaoke {
  char title[32] = "";
  uint32_t titleColor = 0;              // 0xRRGGBB
  uint32_t colors[KARAOKE_ROWS] = {};   // of the rows on the board, top to bottom
  KaraokeEntry entries[MAX_KARAOKE];
  uint8_t n = 0;
};

// Heading "Karaoke", amber, the rows in four colours, no entries.
void karaokeDefaults(Karaoke& k);

// JSON as used by the web page and for storing (format: README, Karaoke).
void karaokeToJson(const Karaoke& k, JsonObject out);

// Start from the defaults, take what `in` has, cut texts; entries without a
// name are left out. false only if `in` isn't an object.
bool karaokeFromJson(JsonVariantConst in, Karaoke& k);
