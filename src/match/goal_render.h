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
// then breaks in one of CONCEDED_ANIMS ways (`seed` % CONCEDED_ANIMS: blown
// up, torn apart, shattered, melted, shot full of holes, down the drain,
// squashed flat, struck by lightning, launched and crashed, knocked down
// by a wrecking ball; the rest of `seed` varies the debris) and leaves its
// remains, "DIE RAUTE KASSIERT!" in St. Pauli's colours, fireworks, and the
// score of `m` (id 0: none, for "Gegentor testen").
static const int CONCEDED_ANIMS = 10;
void drawConceded(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, uint32_t t, uint32_t seed);
