#pragma once
// Event mode: the whole board shows scenes of free text in colour, with
// animations and transitions (src/event_render.cpp), instead of the
// departures, until it is switched off again on the web page. No Arduino
// dependency so it can be unit-tested on the host and used by the preview.
#include <ArduinoJson.h>
#include <stdint.h>

#define MAX_SCENES       8
#define MAX_EVENT_LINES  4
#define MAX_IMAGES      16  // pixel images (src/image_store.h), ids 0..15
#define MAX_IMAGE_FRAMES 8  // frames of an animated one

enum class TextSize : uint8_t { Small, Normal, Large, Huge };
enum class VPos : uint8_t { Top, Middle, Bottom };
enum class HAlign : uint8_t { Left, Center, Right };
// Scroll: through from the right like a ticker; Type: letter by letter
enum class Anim : uint8_t { None, Scroll, Blink, Pulse, Type, Wave };
// how a scene hands over to the next one
enum class Transition : uint8_t { Cut, Slide, Wipe };

#define EVENT_SPEED_DEFAULT 5  // Speed 1..10
#define TRANSITION_MS     600

struct EventLine {
  char text[64] = "";
  uint32_t color = 0xFF7800;  // 0xRRGGBB
  bool rainbow = false;       // colours running through the letters instead of `color`
  TextSize size = TextSize::Normal;
  VPos v = VPos::Middle;
  HAlign h = HAlign::Center;
  Anim anim = Anim::None;
  uint8_t speed = EVENT_SPEED_DEFAULT;  // of the animation and the rainbow, 1 (slow) .. 10
};

struct Scene {
  char name[24] = "";
  uint16_t durS = 8;  // how long it stays before the next one
  uint32_t bg = 0;    // 0xRRGGBB, 0 = black
  int8_t img = -1;    // pixel image over the background, -1 = none
  Transition trans = Transition::Cut;  // to the next scene
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

// 0..359 -> fully saturated 0xRRGGBB
uint32_t hueColor(int hue);

// 0xRRGGBB at `level` (0..256 = full)
uint32_t dimColor(uint32_t rgb, int level);

// JSON as used by the web page and for storing (format: README, Event-Modus).
void eventToJson(const Event& e, JsonObject out);
void sceneToJson(const Scene& sc, JsonObject out);

// Start from the defaults (a scene from an empty one), take what `in` has,
// clamp numbers and cut texts. false only if `in` isn't an object.
bool eventFromJson(JsonVariantConst in, Event& e);
bool sceneFromJson(JsonVariantConst in, Scene& sc);
