#pragma once
// Settings that can be changed on the web page (src/web.cpp) while the board
// runs. Defaults come from config.h. No Arduino dependency so they can be
// unit-tested on the host and used by the preview.
#include <ArduinoJson.h>
#include <stdint.h>
#include <time.h>

#include "icons.h"

#define MAX_PROMOS        20   // built-in (PROMO_TEXTS) + own ones
#define MAX_DIM_WINDOWS    4
#define MAX_CUSTOM_ICONS   8   // tags "[c0]" .. "[c7]"

// Local time span in minutes since midnight; from > to runs over midnight,
// from == to is empty, to = 1440 means "until midnight".
struct TimeWindow {
  uint16_t from = 0, to = 0;
};

struct Promo {
  bool on = true;
  const char* preset = nullptr;  // its PROMO_TEXTS entry for a built-in one
  char icon[8] = "";             // "" or an icon tag ("[ig]", "[c3]"); with `right` also a line text
  char text[64] = "";
  char right[16] = "";           // set: shown like a departure ("sofort" in the time column)
  bool hasWin = false;           // only shown within `win`
  TimeWindow win;
};

struct CustomIcon {
  bool used = false;
  char name[16] = "";
  uint16_t rows[ICON_H] = {};  // bit 11 = left pixel
};

struct DimWindow {
  TimeWindow win;
  uint8_t value = 0;
};

struct Settings {
  bool offEnabled = false;  // panel dark and no API calls within `off`
  TimeWindow off;
  uint8_t brightness = 0;  // 0..255 outside the dim windows
  DimWindow dim[MAX_DIM_WINDOWS];
  uint8_t nDim = 0;
  uint16_t promoShowS = 0;      // how long one promo stays
  uint16_t promoIntervalS = 0;  // average pause between promos, 0 = no promos
  Promo promos[MAX_PROMOS];
  uint8_t nPromos = 0;
  CustomIcon icons[MAX_CUSTOM_ICONS];  // index = number in the tag
};

// config.h values; the built-in promos come first, in PROMO_TEXTS order.
void settingsDefaults(Settings& s);

// Fills `p` from a PROMO_TEXTS entry ("@12-18[gc]|Grilled Cheese|5 min").
void promoFromConfig(const char* text, Promo& p);

bool inWindow(const TimeWindow& w, int minuteOfDay);
int minuteOfDay(time_t now);  // local time

bool isOffTime(const Settings& s, int minuteOfDay);
uint8_t brightnessAt(const Settings& s, int minuteOfDay);

// Custom icon index for "[c3]", -1 if `tag` isn't one.
int customIconIndex(const char* tag);

// JSON as used by the web page and for storing. `forWeb` adds the details of
// the built-in promos and the built-in icons; the stored version only keeps
// which built-in promos are switched off, so new firmware can change them.
void settingsToJson(const Settings& s, JsonObject out, bool forWeb);

// Starts from the defaults, takes what `in` has, clamps numbers and cuts
// texts. Unknown built-in promos are dropped, missing ones stay on.
// false only if `in` isn't an object.
bool settingsFromJson(JsonVariantConst in, Settings& s);
