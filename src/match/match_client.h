#pragma once
// The St.-Pauli-Ticker's match (src/match/match.h) at run time: fetched from
// OpenLigaDB by the network task, copied by the render loop, like the
// departures in src/main.cpp.
#include <stdint.h>

#include "match/match.h"

// Call once in setup().
void matchBegin();

// Network task, call often: while `active` (ticker on, no event, not in the
// off time) the list of matches every MATCH_POLL_IDLE_S, the score every
// MATCH_POLL_LIVE_S while one runs. Switched off it forgets the match.
// A goal counts up the goal sequence if the settings want the animation.
void matchMaintain(bool active);

// The match as the network task has it, for the web page.
const Match& matchNet();

// Network task: the goal animation once, for "Tor testen".
void goalTest();

// Render loop: copies the match into `out` if it changed since `version`,
// which is updated. true if copied.
bool matchFetch(Match& out, uint32_t& version);

// Render loop: true if there was a goal (or a test) since `version`, which
// is updated.
bool goalFetch(uint32_t& version);
