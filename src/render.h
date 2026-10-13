#pragma once
// Draws the board onto any Adafruit_GFX surface. Hardware-independent so the
// same code renders on the LED panels and in the host preview (tools/preview).
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>

#include "departure.h"
#include "settings.h"

struct DisplayStatus {
  bool wifi = false;
  bool timeValid = false;
  bool updating = false;  // firmware update running -> only a notice
};

// 1 in `oneIn` departures plays the departure animation, 0 = never.
// Defaults to DEPART_ANIM_CHANCE (buses: DEPART_ANIM_CHANCE_BUS); the preview
// sets 1 to force it. 0 turns it off for buses too.
// Randomness comes from rand(), seed it once at start-up.
void setDepartAnimChance(int oneIn);

// Forces the promo with this index in Settings::promos (if it is on and its
// time window is open), -1 = random. Only the preview uses it.
void setPromoIndex(int idx);

// `g` must be cleared beforehand. `ms` drives the scrolling texts and the
// departure animation, so call it continuously with a monotonic clock.
// `s`: promos, their icons and timing (brightness is the caller's job).
void renderFrame(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Board& b,
                 const Settings& s, const DisplayStatus& st, time_t now, uint32_t ms);
