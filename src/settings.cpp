#include "settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "parse.h"

static TimeWindow hours(int from, int to) {
  TimeWindow w;
  w.from = (uint16_t)(from * 60);
  w.to = (uint16_t)(to * 60);
  return w;
}

// Copies at most `n` bytes of `in` as sanitized UTF-8 (only characters the
// fonts have).
static void copyText(char* out, size_t outSize, const char* in, size_t n = (size_t)-1) {
  char tmp[128];
  snprintf(tmp, sizeof(tmp), "%.*s", (int)(n < sizeof(tmp) ? n : sizeof(tmp) - 1), in ? in : "");
  sanitizeUtf8(tmp, out, outSize);
}

void promoFromText(const char* text, Promo& p) {
  p = Promo();
  // optional "@HH" or "@HH-HH" prefix
  if (text[0] == '@' && isdigit((unsigned char)text[1])) {
    char* end;
    long from = strtol(text + 1, &end, 10), to = 24;
    if (*end == '-') to = strtol(end + 1, &end, 10);
    p.hasWin = true;
    p.win = hours((int)from, (int)to);
    text = end;
  }
  // "line|destination|time" (line = icon tag or text) or "[ig]text"
  const char* a = strchr(text, '|');
  const char* b = a ? strchr(a + 1, '|') : nullptr;
  if (b) {
    copyText(p.icon, sizeof(p.icon), text, a - text);
    copyText(p.text, sizeof(p.text), a + 1, b - a - 1);
    copyText(p.right, sizeof(p.right), b + 1);
  } else if (text[0] == '[' && strchr(text, ']')) {
    const char* close = strchr(text, ']');
    copyText(p.icon, sizeof(p.icon), text, close - text + 1);
    copyText(p.text, sizeof(p.text), close + 1);
  } else {
    copyText(p.text, sizeof(p.text), text);
  }
}

void settingsDefaults(Settings& s) {
  s = Settings();
#if OFF_START_HOUR >= 0
  s.offEnabled = true;
  s.off = hours(OFF_START_HOUR, OFF_END_HOUR);
#else
  s.off = hours(2, 6);  // preset for the web page
#endif
  s.brightness = BRIGHTNESS_DAY;
#if NIGHT_START_HOUR >= 0
  s.dim[0].win = hours(NIGHT_START_HOUR, NIGHT_END_HOUR);
  s.dim[0].value = BRIGHTNESS_NIGHT;
  s.nDim = 1;
#endif
  s.promoShowS = PROMO_SHOW_S;
  s.promoIntervalS = PROMO_INTERVAL_S;
  s.disruptPasses = DISRUPTION_PASSES;
}

bool inWindow(const TimeWindow& w, int m) {
  if (w.from == w.to) return false;
  return w.from < w.to ? m >= w.from && m < w.to : m >= w.from || m < w.to;
}

int minuteOfDay(time_t now) {
  struct tm lt;
  localtime_r(&now, &lt);
  return lt.tm_hour * 60 + lt.tm_min;
}

// days since 1970-01-01 <-> civil date (proleptic Gregorian)
static int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static void civilFromDays(int32_t z, int& y, int& m, int& d) {
  z += 719468;
  int era = (z >= 0 ? z : z - 146096) / 146097;
  int doe = z - era * 146097;
  int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = yoe + era * 400 + (m <= 2);
}

int32_t localDay(time_t now) {
  struct tm lt;
  localtime_r(&now, &lt);
  return daysFromCivil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
}

int32_t windowStartDay(const TimeWindow& w, int m, int32_t day) {
  return w.from > w.to && m < w.to ? day - 1 : day;
}

int weekday(int32_t day) { return (int)(((day + 3) % 7 + 7) % 7); }  // 1970-01-01 was a Thursday

bool promoOpen(const Promo& p, int m, int32_t day) {
  if (!p.on) return false;
  if (!p.hasWin && p.days == ALL_DAYS) return true;
  if (m < 0) return false;
  if (p.hasWin && !inWindow(p.win, m)) return false;
  int32_t start = p.hasWin ? windowStartDay(p.win, m, day) : day;
  return p.days & (1 << weekday(start));
}

bool isOffTime(const Settings& s, int m) { return s.offEnabled && inWindow(s.off, m); }

uint8_t brightnessAt(const Settings& s, int m, int32_t day) {
  for (int i = 0; i < s.nDim; i++) {
    const DimWindow& d = s.dim[i];
    if (!inWindow(d.win, m)) continue;
    if (day && d.skipDay && d.skipDay == windowStartDay(d.win, m, day)) continue;
    return d.value;
  }
  return s.brightness;
}

uint8_t brightnessToPanel(uint8_t percent) {
  return (uint8_t)((percent > 100 ? 100 : percent) * BRIGHTNESS_MAX / 100);
}

int customIconIndex(const char* tag) {
  if (tag[0] != '[' || tag[1] != 'c' || !isdigit((unsigned char)tag[2]) || tag[3] != ']' || tag[4]) return -1;
  int i = tag[2] - '0';
  return i < MAX_CUSTOM_ICONS ? i : -1;
}

// ---- JSON ----------------------------------------------------------------------

static void formatTime(char* out, size_t n, uint16_t m) { snprintf(out, n, "%02u:%02u", m / 60, m % 60); }

// "HH:MM" -> minutes (0..1440), -1 if malformed
static int parseTime(const char* s) {
  if (!s) return -1;
  int h, m;
  char end;
  if (sscanf(s, "%d:%d%c", &h, &m, &end) != 2 || h < 0 || m < 0 || m > 59 || h * 60 + m > 1440) return -1;
  return h * 60 + m;
}

// "YYYY-MM-DD" -> localDay() value, 0 if missing or malformed
static int32_t parseDate(const char* s) {
  if (!s) return 0;
  int y, m, d;
  char end;
  if (sscanf(s, "%d-%d-%d%c", &y, &m, &d, &end) != 3 || y < 2000 || y > 2200 || m < 1 || m > 12 || d < 1 || d > 31)
    return 0;
  return daysFromCivil(y, m, d);
}

static void windowToJson(const TimeWindow& w, JsonObject o) {
  char t[8];
  formatTime(t, sizeof(t), w.from);
  o["from"] = (const char*)t;
  formatTime(t, sizeof(t), w.to);
  o["to"] = (const char*)t;
}

static bool windowFromJson(JsonVariantConst o, TimeWindow& w) {
  int from = parseTime(o["from"]), to = parseTime(o["to"]);
  if (from < 0 || to < 0) return false;
  w.from = (uint16_t)from;
  w.to = (uint16_t)to;
  return true;
}

static uint32_t clampU(JsonVariantConst v, uint32_t lo, uint32_t hi, uint32_t def) {
  if (!v.is<long>()) return def;
  long x = v.as<long>();
  return x < (long)lo ? lo : x > (long)hi ? hi : (uint32_t)x;
}

// (const char*): ArduinoJson keeps char arrays by pointer like literals
static void promoFieldsToJson(const Promo& p, JsonObject o) {
  o["icon"] = (const char*)p.icon;
  o["text"] = (const char*)p.text;
  o["right"] = (const char*)p.right;
  if (p.hasWin) windowToJson(p.win, o["win"].to<JsonObject>());
  if (p.days != ALL_DAYS) o["days"] = p.days;
}

// Takes icon, text, right and window of `o`, keeps `on` of `p`.
static void promoFieldsFromJson(JsonVariantConst o, Promo& p) {
  copyText(p.text, sizeof(p.text), o["text"] | "");
  copyText(p.right, sizeof(p.right), o["right"] | "");
  const char* icon = o["icon"] | "";
  p.icon[0] = 0;
  if (findPresetIcon(icon) || customIconIndex(icon) >= 0) snprintf(p.icon, sizeof(p.icon), "%s", icon);
  p.hasWin = o["win"].is<JsonObjectConst>() && windowFromJson(o["win"], p.win);
  if (!p.hasWin) p.win = TimeWindow();
  uint8_t days = (uint8_t)(clampU(o["days"], 0, ALL_DAYS, ALL_DAYS) & ALL_DAYS);
  p.days = days ? days : ALL_DAYS;  // none at all: rather every day than never
}

void settingsToJson(const Settings& s, JsonObject out, bool forWeb) {
  JsonObject off = out["off"].to<JsonObject>();
  off["on"] = s.offEnabled;
  windowToJson(s.off, off);
  out["brightnessPct"] = s.brightness;
  JsonArray dim = out["dim"].to<JsonArray>();
  for (int i = 0; i < s.nDim; i++) {
    JsonObject d = dim.add<JsonObject>();
    windowToJson(s.dim[i].win, d);
    d["pct"] = s.dim[i].value;
    if (s.dim[i].skipDay) {
      int y, m, dd;
      char t[12];
      civilFromDays(s.dim[i].skipDay, y, m, dd);
      snprintf(t, sizeof(t), "%04d-%02d-%02d", y, m, dd);
      d["skip"] = (const char*)t;
    }
  }
  out["promoShowS"] = s.promoShowS;
  out["promoIntervalS"] = s.promoIntervalS;
  out["promoFullRow"] = s.promoFullRow;
  out["disruptOn"] = s.disruptOn;
  out["disruptPasses"] = s.disruptPasses;

  JsonArray promos = out["promos"].to<JsonArray>();
  for (int i = 0; i < s.nPromos; i++) {
    const Promo& p = s.promos[i];
    JsonObject o = promos.add<JsonObject>();
    o["on"] = p.on;
    promoFieldsToJson(p, o);
  }

  JsonArray icons = out["icons"].to<JsonArray>();
  for (int i = 0; i < MAX_CUSTOM_ICONS; i++) {
    const CustomIcon& ic = s.icons[i];
    if (!ic.used) continue;
    JsonObject o = icons.add<JsonObject>();
    o["id"] = i;
    o["name"] = (const char*)ic.name;
    JsonArray rows = o["rows"].to<JsonArray>();
    for (int r = 0; r < ICON_H; r++) rows.add(ic.rows[r]);
  }

  if (!forWeb) return;
  out["brightnessDefaultPct"] = BRIGHTNESS_DAY;
  JsonArray preset = out["presetIcons"].to<JsonArray>();
  for (int i = 0; i < N_PRESET_ICONS; i++) {
    const PresetIcon& ic = PRESET_ICONS[i];
    JsonObject o = preset.add<JsonObject>();
    o["tag"] = ic.tag;
    o["name"] = ic.name;
    JsonArray rows = o["rows"].to<JsonArray>();
    for (int r = 0; r < ic.h; r++) rows.add(ic.rows[r]);
  }
}

bool settingsFromJson(JsonVariantConst in, Settings& s) {
  settingsDefaults(s);
  if (!in.is<JsonObjectConst>()) return false;

  JsonVariantConst off = in["off"];
  if (off.is<JsonObjectConst>()) {
    s.offEnabled = off["on"] | s.offEnabled;
    windowFromJson(off, s.off);
  }
  // percent; build 10 stored 0..255 under "brightness"/"value", those are dropped
  s.brightness = (uint8_t)clampU(in["brightnessPct"], 1, 100, s.brightness);
  if (in["dim"].is<JsonArrayConst>()) {
    s.nDim = 0;
    for (JsonVariantConst d : in["dim"].as<JsonArrayConst>()) {
      if (s.nDim >= MAX_DIM_WINDOWS) break;
      DimWindow& w = s.dim[s.nDim];
      if (!windowFromJson(d, w.win)) continue;
      w.value = (uint8_t)clampU(d["pct"], 1, 100, s.brightness);
      w.skipDay = parseDate(d["skip"]);
      s.nDim++;
    }
  }
  s.promoShowS = (uint16_t)clampU(in["promoShowS"], 3, 600, s.promoShowS);
  s.promoIntervalS = (uint16_t)clampU(in["promoIntervalS"], 0, 3600, s.promoIntervalS);
  s.promoFullRow = in["promoFullRow"] | s.promoFullRow;
  s.disruptOn = in["disruptOn"] | s.disruptOn;
  s.disruptPasses = (uint8_t)clampU(in["disruptPasses"], 1, 10, s.disruptPasses);

  if (in["promos"].is<JsonArrayConst>()) {
    for (JsonVariantConst o : in["promos"].as<JsonArrayConst>()) {
      if (s.nPromos >= MAX_PROMOS) break;
      Promo p;
      // stored by older firmware: a built-in promo, unedited only its text
      const char* preset = o["preset"];
      if (preset && !o["text"].is<const char*>()) promoFromText(preset, p);
      else promoFieldsFromJson(o, p);
      p.on = o["on"] | true;
      if (!p.text[0] && !p.icon[0]) continue;
      s.promos[s.nPromos++] = p;
    }
  }

  if (in["icons"].is<JsonArrayConst>()) {
    for (JsonVariantConst o : in["icons"].as<JsonArrayConst>()) {
      if (!o["id"].is<int>()) continue;
      int id = o["id"];
      if (id < 0 || id >= MAX_CUSTOM_ICONS) continue;
      CustomIcon& ic = s.icons[id];
      ic.used = true;
      copyText(ic.name, sizeof(ic.name), o["name"] | "");
      JsonArrayConst rows = o["rows"];
      for (int r = 0; r < ICON_H; r++) ic.rows[r] = (uint16_t)(rows[r] | 0) & ((1 << ICON_W) - 1);
    }
  }
  return true;
}
