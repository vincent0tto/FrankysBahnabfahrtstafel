#include "karaoke.h"

#include <stdio.h>

#include "event.h"

static const uint32_t DEFAULT_COLORS[KARAOKE_ROWS] = {0xFFD400, 0x29C6FF, 0xFF3BD0, 0x4BE04B};

void karaokeDefaults(Karaoke& k) {
  k = Karaoke();
  snprintf(k.title, sizeof(k.title), "Karaoke");
  k.titleColor = 0xFF7800;
  for (int i = 0; i < KARAOKE_ROWS; i++) k.colors[i] = DEFAULT_COLORS[i];
}

void karaokeToJson(const Karaoke& k, JsonObject out) {
  out["title"] = (const char*)k.title;
  colorToJson(k.titleColor, out, "titleColor");
  JsonArray colors = out["colors"].to<JsonArray>();
  for (int i = 0; i < KARAOKE_ROWS; i++) {
    char t[8];
    snprintf(t, sizeof(t), "#%06lx", (unsigned long)(k.colors[i] & 0xFFFFFF));
    colors.add((const char*)t);
  }
  JsonArray entries = out["entries"].to<JsonArray>();
  for (int i = 0; i < k.n; i++) {
    JsonObject o = entries.add<JsonObject>();
    o["name"] = (const char*)k.entries[i].name;
    o["song"] = (const char*)k.entries[i].song;
  }
}

bool karaokeFromJson(JsonVariantConst in, Karaoke& k) {
  karaokeDefaults(k);
  if (!in.is<JsonObjectConst>()) return false;
  if (in["title"].is<const char*>()) copyText(k.title, sizeof(k.title), in["title"]);
  k.titleColor = colorFromJson(in["titleColor"], k.titleColor);
  JsonArrayConst colors = in["colors"];
  for (int i = 0; i < KARAOKE_ROWS && i < (int)colors.size(); i++) k.colors[i] = colorFromJson(colors[i], k.colors[i]);
  for (JsonVariantConst o : in["entries"].as<JsonArrayConst>()) {
    if (k.n >= MAX_KARAOKE) break;
    KaraokeEntry& e = k.entries[k.n];
    copyText(e.name, sizeof(e.name), o["name"]);
    copyText(e.song, sizeof(e.song), o["song"]);
    if (e.name[0]) k.n++;
    else e = KaraokeEntry();
  }
  return true;
}
