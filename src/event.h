#pragma once
// Event mode: the whole board shows scenes of free text in colour
// (src/event_render.cpp) instead of the departures, until it is switched off
// again on the web page. No Arduino dependency so it can be unit-tested on
// the host and used by the preview.
#include <ArduinoJson.h>
#include <stdint.h>

#define MAX_SCENES       8
#define MAX_EVENT_LINES  4

enum class TextSize : uint8_t { Small, Normal, Large, Huge };
enum class VPos : uint8_t { Top, Middle, Bottom };
enum class HAlign : uint8_t { Left, Center, Right };

struct EventLine {
  char text[64] = "";
  uint32_t color = 0xFF7800;  // 0xRRGGBB
  TextSize size = TextSize::Normal;
  VPos v = VPos::Middle;
  HAlign h = HAlign::Center;
};

struct Scene {
  char name[24] = "";
  uint16_t durS = 8;  // how long it stays before the next one
  uint32_t bg = 0;    // 0xRRGGBB, 0 = black
  EventLine lines[MAX_EVENT_LINES];
  uint8_t nLines = 0;
};

struct Event {
  bool on = false;          // the board shows the scenes instead of the departures
  uint8_t brightness = 0;   // percent (1..100) of BRIGHTNESS_MAX, no dim windows
  Scene scenes[MAX_SCENES];
  uint8_t nScenes = 0;
};

// Off, BRIGHTNESS_DAY, one example scene.
void eventDefaults(Event& e);

// 0xRRGGBB -> the panel's RGB565
uint16_t rgb565(uint32_t rgb);

// The scene shown `ms` after the scenes started (they run in a loop), -1 if
// there is none; `sceneMs` gets the time since that scene started.
int eventSceneAt(const Event& e, uint32_t ms, uint32_t& sceneMs);

// JSON as used by the web page and for storing (format: README, Event-Modus).
void eventToJson(const Event& e, JsonObject out);
void sceneToJson(const Scene& sc, JsonObject out);

// Start from the defaults (a scene from an empty one), take what `in` has,
// clamp numbers and cut texts. false only if `in` isn't an object.
bool eventFromJson(JsonVariantConst in, Event& e);
bool sceneFromJson(JsonVariantConst in, Scene& sc);
