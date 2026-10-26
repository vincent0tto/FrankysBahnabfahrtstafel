#pragma once
// Draws the board onto any Adafruit_GFX surface. Hardware-independent so the
// same code renders on the LED panels and in the host preview (tools/preview).
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <time.h>

#include "core/settings.h"
#include "departures/departure.h"
#include "match/match.h"

struct DisplayStatus {
  bool wifi = false;
  bool timeValid = false;
  bool updating = false;  // firmware update running -> only a notice
};

// Overrides Settings::animChance/animChanceBus: 1 in `oneIn` departures plays
// the departure animation, 0 = never, -1 = as in the settings (default). Only
// the preview uses it (1 forces it).
// Randomness comes from rand(), seed it once at start-up.
void setDepartAnimChance(int oneIn);

// Forces the promo with this index in Settings::promos (if it is on and its
// time window is open), -1 = random. Only the preview uses it.
void setPromoIndex(int idx);

// `g` must be cleared beforehand. `ms` drives the scrolling texts and the
// departure animation, so call it continuously with a monotonic clock.
// `s`: promos, their icons and timing (brightness is the caller's job).
// `m`: the St.-Pauli-Ticker's match (id 0: none), live in the 4th row,
// afterwards its final score as one more promo. `bouncer` (nullptr: none):
// the "Rausschmeißer" scrolling through the 4th row instead of all that and
// the status messages, `bouncerMs` after it started.
void renderFrame(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Board& b,
                 const Settings& s, const DisplayStatus& st, time_t now, uint32_t ms, const Match& m = Match(),
                 const Promo* bouncer = nullptr, uint32_t bouncerMs = 0);

// Fills the board with `p` (icon and text, as large as fits, on up to 4
// lines; too long even then: scrolling through the middle, `t` ms after it
// came up), in `rgb` (0xRRGGBB). For the "Rausschmeißer" (Settings::bouncer).
void renderCallout(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint32_t rgb, const Settings& s, const Promo& p,
                   uint32_t t);

// `p` (icon and text) as high as the board, scrolling through from the right
// again and again, a quarter of the board apart, `t` ms after it came up,
// switching between `rgb` and `rgb2` (0xRRGGBB) every LAST_ROUND_COLOR_MS.
// For the "Letzte Runde" (Settings::lastRound).
void renderMarquee(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint32_t rgb, uint32_t rgb2, const Settings& s,
                   const Promo& p, uint32_t t);
