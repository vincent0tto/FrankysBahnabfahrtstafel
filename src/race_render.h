#pragma once
// Draws the tram race (src/race.h) onto any Adafruit_GFX surface, like
// depart_anim.h: no state, the same on the panels and in the host preview.
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "race.h"

// `g` must be cleared beforehand. `t`: ms since the race was started
// (0..raceTotalMs): countdown, race, winner.
void drawRace(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Race& r, uint32_t t);

// A firework burst `age` ms old (it lasts 1200) around (cx, cy).
void drawFireworkBurst(Adafruit_GFX& g, uint16_t color, int cx, int cy, uint32_t age);
