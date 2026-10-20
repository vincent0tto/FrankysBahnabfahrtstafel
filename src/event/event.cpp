#include "event/event.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "departures/parse.h"

static const char* const SIZE_NAMES[] = {"small", "normal", "large", "huge"};
static const char* const VPOS_NAMES[] = {"top", "middle", "bottom"};
static const char* const HALIGN_NAMES[] = {"left", "center", "right"};
static const char* const ANIM_NAMES[] = {"none", "scroll", "blink", "pulse", "type", "wave"};
static const char* const TRANS_NAMES[] = {"cut", "slide", "wipe"};

void eventDefaults(Event& e) {
  e = Event();
  e.brightness = BRIGHTNESS_DAY;
  Scene& sc = e.scenes[e.nScenes++];
  snprintf(sc.name, sizeof(sc.name), "Willkommen");
  EventLine& a = sc.lines[sc.nLines++];
  snprintf(a.text, sizeof(a.text), "Willkommen im");
  a.v = VPos::Top;
  EventLine& b = sc.lines[sc.nLines++];
  snprintf(b.text, sizeof(b.text), "FRANKY'S");
  b.size = TextSize::Huge;
  b.v = VPos::Bottom;
}

uint16_t rgb565(uint32_t rgb) {
  uint8_t r = rgb >> 16, g = rgb >> 8, b = rgb;
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint32_t hueColor(int hue) {
  hue = ((hue % 360) + 360) % 360;
  int x = (hue % 60) * 255 / 60;  // rising part of the current sixth
  uint8_t r, g, b;
  switch (hue / 60) {
    case 0: r = 255, g = x, b = 0; break;
    case 1: r = 255 - x, g = 255, b = 0; break;
    case 2: r = 0, g = 255, b = x; break;
    case 3: r = 0, g = 255 - x, b = 255; break;
    case 4: r = x, g = 0, b = 255; break;
    default: r = 255, g = 0, b = 255 - x; break;
  }
  return (uint32_t)r << 16 | g << 8 | b;
}

uint32_t dimColor(uint32_t rgb, int level) {
  uint32_t r = (rgb >> 16 & 0xFF) * level >> 8, g = (rgb >> 8 & 0xFF) * level >> 8, b = (rgb & 0xFF) * level >> 8;
  return r << 16 | g << 8 | b;
}

int eventSceneAt(const Event& e, uint32_t ms, uint32_t& sceneMs) {
  uint32_t total = 0;
  for (int i = 0; i < e.nScenes; i++) total += e.scenes[i].durS * 1000UL;
  if (total == 0) return -1;
  uint32_t t = ms % total;
  for (int i = 0; i < e.nScenes; i++) {
    uint32_t d = e.scenes[i].durS * 1000UL;
    if (t < d) {
      sceneMs = t;
      return i;
    }
    t -= d;
  }
  return -1;  // not reached
}

// ---- JSON ----------------------------------------------------------------------

void copyText(char* out, size_t outSize, const char* in) {
  char tmp[128];
  snprintf(tmp, sizeof(tmp), "%s", in ? in : "");
  sanitizeUtf8(tmp, out, outSize);
}

void colorToJson(uint32_t rgb, JsonObject o, const char* key) {
  char t[8];
  snprintf(t, sizeof(t), "#%06lx", (unsigned long)(rgb & 0xFFFFFF));
  o[key] = (const char*)t;
}

uint32_t colorFromJson(JsonVariantConst v, uint32_t def) {
  const char* s = v;
  if (!s || s[0] != '#' || strlen(s) != 7) return def;
  char* end;
  unsigned long x = strtoul(s + 1, &end, 16);
  return *end ? def : (uint32_t)x;
}

static uint32_t clampU(JsonVariantConst v, uint32_t lo, uint32_t hi, uint32_t def) {
  if (!v.is<long>()) return def;
  long x = v.as<long>();
  return x < (long)lo ? lo : x > (long)hi ? hi : (uint32_t)x;
}

// index of `v` in `names`, `def` if it isn't one
static uint8_t nameIndex(JsonVariantConst v, const char* const* names, int n, uint8_t def) {
  const char* s = v;
  for (int i = 0; s && i < n; i++)
    if (strcmp(s, names[i]) == 0) return (uint8_t)i;
  return def;
}

void sceneToJson(const Scene& sc, JsonObject out) {
  out["name"] = (const char*)sc.name;
  out["durS"] = sc.durS;
  colorToJson(sc.bg, out, "bg");
  if (sc.img >= 0) out["img"] = sc.img;
  out["trans"] = TRANS_NAMES[(int)sc.trans];
  JsonArray lines = out["lines"].to<JsonArray>();
  for (int i = 0; i < sc.nLines; i++) {
    const EventLine& l = sc.lines[i];
    JsonObject o = lines.add<JsonObject>();
    o["text"] = (const char*)l.text;
    if (l.rainbow) o["color"] = "rainbow";
    else colorToJson(l.color, o, "color");
    o["size"] = SIZE_NAMES[(int)l.size];
    o["v"] = VPOS_NAMES[(int)l.v];
    o["h"] = HALIGN_NAMES[(int)l.h];
    o["anim"] = ANIM_NAMES[(int)l.anim];
    o["speed"] = l.speed;
  }
}

void eventToJson(const Event& e, JsonObject out) {
  out["on"] = e.on;
  out["karaoke"] = e.karaoke;
  out["brightnessPct"] = e.brightness;
  JsonArray scenes = out["scenes"].to<JsonArray>();
  for (int i = 0; i < e.nScenes; i++) sceneToJson(e.scenes[i], scenes.add<JsonObject>());
}

bool sceneFromJson(JsonVariantConst in, Scene& sc) {
  sc = Scene();
  if (!in.is<JsonObjectConst>()) return false;
  copyText(sc.name, sizeof(sc.name), in["name"]);
  sc.durS = (uint16_t)clampU(in["durS"], 2, 600, sc.durS);
  sc.bg = colorFromJson(in["bg"], sc.bg);
  int img = in["img"] | -1;
  sc.img = (int8_t)(img >= 0 && img < MAX_IMAGES ? img : -1);
  sc.trans = (Transition)nameIndex(in["trans"], TRANS_NAMES, 3, (uint8_t)sc.trans);
  for (JsonVariantConst o : in["lines"].as<JsonArrayConst>()) {
    if (sc.nLines >= MAX_EVENT_LINES) break;
    if (!o.is<JsonObjectConst>()) continue;
    EventLine& l = sc.lines[sc.nLines++];
    copyText(l.text, sizeof(l.text), o["text"]);
    l.rainbow = o["color"] == "rainbow";
    l.color = colorFromJson(o["color"], l.color);
    l.size = (TextSize)nameIndex(o["size"], SIZE_NAMES, 4, (uint8_t)l.size);
    l.v = (VPos)nameIndex(o["v"], VPOS_NAMES, 3, (uint8_t)l.v);
    l.h = (HAlign)nameIndex(o["h"], HALIGN_NAMES, 3, (uint8_t)l.h);
    l.anim = (Anim)nameIndex(o["anim"], ANIM_NAMES, 6, (uint8_t)l.anim);
    l.speed = (uint8_t)clampU(o["speed"], 1, 10, l.speed);
  }
  return true;
}

bool eventFromJson(JsonVariantConst in, Event& e) {
  eventDefaults(e);
  if (!in.is<JsonObjectConst>()) return false;
  e.on = in["on"] | e.on;
  e.karaoke = in["karaoke"] | e.karaoke;
  e.brightness = (uint8_t)clampU(in["brightnessPct"], 1, 100, e.brightness);
  if (in["scenes"].is<JsonArrayConst>()) {
    e.nScenes = 0;
    for (JsonVariantConst o : in["scenes"].as<JsonArrayConst>()) {
      if (e.nScenes >= MAX_SCENES) break;
      if (sceneFromJson(o, e.scenes[e.nScenes])) e.nScenes++;
    }
  }
  return true;
}
