#pragma once
#include <stdint.h>
#include <time.h>

#include "core/settings.h"
#include "departures/departure.h"
#include "display/render.h"
#include "event/event.h"
#include "karaoke/karaoke.h"
#include "match/match.h"
#include "race/race.h"

bool displayBegin();

// Draws one complete frame into the back buffer and flips it, with the
// brightness from `s`. Call ~30x per second (scrolling texts); everything
// else is derived from `now`, so cached departures keep counting down while
// the API is down. Within the off time the panel stays dark and nothing is
// drawn; returns true then, so the caller can call less often.
// While `ev` is on it shows the event instead, `eventMs` after its scenes
// started, with its own brightness and no off time; with ev.karaoke the
// list `k` instead of the scenes. A `race` (nullptr: none)
// goes over both, `raceMs` after it started, also in the off time; next
// comes the goal animation, `goalMs` after the goal (NO_GOAL: none). `m`:
// the St.-Pauli-Ticker's match for the 4th row (id 0: none).
bool displayRender(const Board& b, const Settings& s, const Event& ev, const Karaoke& k, const Match& m,
                   const DisplayStatus& st, time_t now, uint32_t ms, uint32_t eventMs, const Race* race,
                   uint32_t raceMs, uint32_t goalMs);
