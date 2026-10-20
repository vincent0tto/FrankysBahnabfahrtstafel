#pragma once
// Settings that can be changed on the web page (src/web.cpp) while the board
// runs. Defaults come from config.h. No Arduino dependency so they can be
// unit-tested on the host and used by the preview.
#include <ArduinoJson.h>
#include <stdint.h>
#include <time.h>

#include "icons.h"

#define MAX_PROMOS        20
#define MAX_DIM_WINDOWS    4
#define MAX_CUSTOM_ICONS   8   // tags "[c0]" .. "[c7]"
#define ALL_DAYS        0x7F   // Promo::days: bit 0 = Monday .. bit 6 = Sunday

// Local time span in minutes since midnight; from > to runs over midnight,
// from == to is empty, to = 1440 means "until midnight".
struct TimeWindow {
  uint16_t from = 0, to = 0;
};

struct Promo {
  bool on = true;
  char icon[8] = "";             // "" or an icon tag ("[ig]", "[c3]"); with `right` also a line text
  char text[64] = "";
  char right[16] = "";           // set: shown like a departure ("sofort" in the time column)
  bool hasWin = false;           // only shown within `win`
  TimeWindow win;
  uint8_t days = ALL_DAYS;       // only shown on these weekdays (never 0)
};

struct CustomIcon {
  bool used = false;
  char name[16] = "";
  uint16_t rows[ICON_H] = {};  // bit 11 = left pixel
};

struct DimWindow {
  TimeWindow win;
  uint8_t value = 0;  // percent
  int32_t skipDay = 0;  // localDay() on which the skipped occurrence starts, 0 = none
};

struct Settings {
  bool offEnabled = false;  // panel dark and no API calls within `off`
  TimeWindow off;
  uint8_t brightness = 0;  // percent (1..100) of BRIGHTNESS_MAX outside the dim windows
  DimWindow dim[MAX_DIM_WINDOWS];
  uint8_t nDim = 0;
  uint16_t promoShowS = 0;      // how long one promo stays
  uint16_t promoIntervalS = 0;  // average pause between promos, 0 = no promos
  bool promoFullRow = false;    // 4th row shows promos back to back (no pause) instead of
                                // its departure, as long as one is open
  bool promoWhite = false;      // with promoFullRow the promos are white instead of amber
  bool disruptOn = true;        // VRN disruption texts
  uint8_t disruptPasses = 0;    // how often one showing scrolls through them (1..10)
  uint8_t animChance = 0;       // 1 in N tram departures plays the departure animation, 0 = never
  uint8_t animChanceBus = 0;    // same for buses
  Promo promos[MAX_PROMOS];
  uint8_t nPromos = 0;
  CustomIcon icons[MAX_CUSTOM_ICONS];  // index = number in the tag
};

// config.h values, no promos.
void settingsDefaults(Settings& s);

// Fills `p` from a text in the former PROMO_TEXTS form
// ("@12-18[gc]|Grilled Cheese|5 min"), as older firmware stored built-in promos.
void promoFromText(const char* text, Promo& p);

bool inWindow(const TimeWindow& w, int minuteOfDay);
int minuteOfDay(time_t now);  // local time
int32_t localDay(time_t now);  // local date as days since 1970-01-01

// localDay() on which the occurrence of `w` that covers `minuteOfDay` of
// `day` started (the day before for the part after midnight)
int32_t windowStartDay(const TimeWindow& w, int minuteOfDay, int32_t day);

// 0 = Monday .. 6 = Sunday for a localDay()
int weekday(int32_t day);

// Whether `p` may be shown at `minuteOfDay` (-1: time unknown) of `day`. A
// time window over midnight counts for the weekday it starts on.
bool promoOpen(const Promo& p, int minuteOfDay, int32_t day);

bool isOffTime(const Settings& s, int minuteOfDay);
// percent; dim windows skipped for the occurrence that started on `day`
// don't count (day 0: ignore skips)
uint8_t brightnessAt(const Settings& s, int minuteOfDay, int32_t day = 0);

// Percent -> panel brightness 0..BRIGHTNESS_MAX
uint8_t brightnessToPanel(uint8_t percent);

// Custom icon index for "[c3]", -1 if `tag` isn't one.
int customIconIndex(const char* tag);

// JSON as used by the web page and for storing. `forWeb` adds the built-in
// icons.
void settingsToJson(const Settings& s, JsonObject out, bool forWeb);

// Starts from the defaults, takes what `in` has, clamps numbers and cuts
// texts. Built-in promos stored by older firmware become ordinary ones.
// false only if `in` isn't an object.
bool settingsFromJson(JsonVariantConst in, Settings& s);
