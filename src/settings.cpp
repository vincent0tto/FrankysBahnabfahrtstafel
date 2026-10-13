#include "settings.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "parse.h"

static const char* const PRESET_PROMOS[] = PROMO_TEXTS;
static const int N_PRESET_PROMOS = sizeof(PRESET_PROMOS) / sizeof(PRESET_PROMOS[0]);

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

void promoFromConfig(const char* text, Promo& p) {
  p = Promo();
  p.preset = text;
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
  for (int i = 0; i < N_PRESET_PROMOS && s.nPromos < MAX_PROMOS; i++)
    promoFromConfig(PRESET_PROMOS[i], s.promos[s.nPromos++]);
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

bool isOffTime(const Settings& s, int m) { return s.offEnabled && inWindow(s.off, m); }

uint8_t brightnessAt(const Settings& s, int m) {
  for (int i = 0; i < s.nDim; i++)
    if (inWindow(s.dim[i].win, m)) return s.dim[i].value;
  return s.brightness;
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

static void windowToJson(const TimeWindow& w, JsonObject o) {
  char t[8];
  formatTime(t, sizeof(t), w.from);
  o["from"] = t;
  formatTime(t, sizeof(t), w.to);
  o["to"] = t;
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

void settingsToJson(const Settings& s, JsonObject out, bool forWeb) {
  JsonObject off = out["off"].to<JsonObject>();
  off["on"] = s.offEnabled;
  windowToJson(s.off, off);
  out["brightness"] = s.brightness;
  JsonArray dim = out["dim"].to<JsonArray>();
  for (int i = 0; i < s.nDim; i++) {
    JsonObject d = dim.add<JsonObject>();
    windowToJson(s.dim[i].win, d);
    d["value"] = s.dim[i].value;
  }
  out["promoShowS"] = s.promoShowS;
  out["promoIntervalS"] = s.promoIntervalS;

  JsonArray promos = out["promos"].to<JsonArray>();
  for (int i = 0; i < s.nPromos; i++) {
    const Promo& p = s.promos[i];
    JsonObject o = promos.add<JsonObject>();
    o["on"] = p.on;
    if (p.preset) {
      o["preset"] = p.preset;  // its PROMO_TEXTS entry identifies it
      if (!forWeb) continue;
    }
    o["icon"] = p.icon;
    o["text"] = p.text;
    o["right"] = p.right;
    if (p.hasWin) windowToJson(p.win, o["win"].to<JsonObject>());
  }

  JsonArray icons = out["icons"].to<JsonArray>();
  for (int i = 0; i < MAX_CUSTOM_ICONS; i++) {
    const CustomIcon& ic = s.icons[i];
    if (!ic.used) continue;
    JsonObject o = icons.add<JsonObject>();
    o["id"] = i;
    o["name"] = ic.name;
    JsonArray rows = o["rows"].to<JsonArray>();
    for (int r = 0; r < ICON_H; r++) rows.add(ic.rows[r]);
  }

  if (!forWeb) return;
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
  s.brightness = (uint8_t)clampU(in["brightness"], 1, 255, s.brightness);
  if (in["dim"].is<JsonArrayConst>()) {
    s.nDim = 0;
    for (JsonVariantConst d : in["dim"].as<JsonArrayConst>()) {
      if (s.nDim >= MAX_DIM_WINDOWS) break;
      DimWindow& w = s.dim[s.nDim];
      if (!windowFromJson(d, w.win)) continue;
      w.value = (uint8_t)clampU(d["value"], 1, 255, s.brightness);
      s.nDim++;
    }
  }
  s.promoShowS = (uint16_t)clampU(in["promoShowS"], 3, 600, s.promoShowS);
  s.promoIntervalS = (uint16_t)clampU(in["promoIntervalS"], 0, 3600, s.promoIntervalS);

  if (in["promos"].is<JsonArrayConst>()) {
    // built-in ones keep their place and text, only "on" is taken
    int nPreset = s.nPromos;
    for (JsonVariantConst o : in["promos"].as<JsonArrayConst>()) {
      const char* preset = o["preset"];
      if (preset) {
        for (int i = 0; i < nPreset; i++)
          if (strcmp(s.promos[i].preset, preset) == 0) s.promos[i].on = o["on"] | true;
        continue;
      }
      if (s.nPromos >= MAX_PROMOS) continue;
      Promo p;
      p.on = o["on"] | true;
      copyText(p.text, sizeof(p.text), o["text"] | "");
      copyText(p.right, sizeof(p.right), o["right"] | "");
      const char* icon = o["icon"] | "";
      if (findPresetIcon(icon) || customIconIndex(icon) >= 0) snprintf(p.icon, sizeof(p.icon), "%s", icon);
      if (o["win"].is<JsonObjectConst>()) p.hasWin = windowFromJson(o["win"], p.win);
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
