#pragma once
// The Fußball tickers' animations (src/match/match.h) over the whole board, like the race (src/race/race_render.h): no state, the same on the panels
// and in the host preview.
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <stdint.h>

#include "match/match.h"

static const uint32_t GOAL_ANIM_MS = 8000;
static const uint32_t NO_GOAL = UINT32_MAX;  // displayRender(): no goal animation

// `g` must be cleared beforehand. `t`: ms since the goal (0..GOAL_ANIM_MS):
// "TOR!" growing to "TOOOOR!", blinking in brown, white and red, fireworks,
// and the score of `m` (id 0: only the team's name, for "Tor testen").
void drawGoal(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, uint32_t t);

// The HSV conceded a goal, `t` as above: a ball flies into its Raute, which
// shakes, "DIE RAUTE KASSIERT!" in St. Pauli's colours, fireworks, and the
// score of `m` (id 0: "Hamburger SV").
void drawConceded(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, uint32_t t);
