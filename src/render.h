#pragma once
// Draws the board onto any Adafruit_GFX surface. Hardware-independent so the
// same code renders on the LED panels and in the host preview (tools/preview).
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>

#include "departure.h"

struct DisplayStatus {
  bool wifi = false;
  bool timeValid = false;
};

// 1 in `oneIn` departures plays the departure animation, 0 = never.
// Defaults to DEPART_ANIM_CHANCE (buses: DEPART_ANIM_CHANCE_BUS); the preview
// sets 1 to force it. 0 turns it off for buses too.
// Randomness comes from rand(), seed it once at start-up.
void setDepartAnimChance(int oneIn);

// Forces the promo with this index in PROMO_TEXTS (if its time window is
// open), -1 = random. Only the preview uses it.
void setPromoIndex(int idx);

// `g` must be cleared beforehand. `ms` drives the scrolling texts and the
// departure animation, so call it continuously with a monotonic clock.
void renderFrame(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Board& b,
                 const DisplayStatus& st, time_t now, uint32_t ms);
