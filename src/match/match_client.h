#pragma once
// The Fußball tickers' matches (src/match/match.h) at run time: fetched from
// ESPN by the network task, copied by the render loop, like the
// departures in src/main.cpp.
#include <stdint.h>

#include "match/match.h"

// St. Pauli: score in the 4th row and an animation for its goals; the HSV:
// only an animation for the goals it concedes.
enum MatchTeam : uint8_t { TEAM_PAULI, TEAM_HSV, N_TEAMS };

// Call once in setup().
void matchBegin();

// Network task, call often: for each team switched on (`active[team]`: its
// ticker on, no event, not in the off time) its list of matches every
// MATCH_POLL_IDLE_S. While matches run, one score every MATCH_POLL_LIVE_S,
// the teams in turn; two matches on the same scoreboard come with one fetch.
// Switched off a team forgets its match. A goal (the HSV: conceded) counts
// up the team's goal sequence if the settings want the animation.
void matchMaintain(const bool active[N_TEAMS]);

// The team's match as the network task has it, for the web page.
const Match& matchNet(MatchTeam team);

// Network task: the team's goal animation once, for "Tor testen".
void goalTest(MatchTeam team);

// Render loop: copies St. Pauli's match into `out` if it changed since
// `version`, which is updated. true if copied.
bool matchFetch(Match& out, uint32_t& version);

// Render loop: true if there was a goal of `team` (or a test) since
// `version`, which is updated; then `m` is its match as of that goal (none
// for a test).
bool goalFetch(MatchTeam team, uint32_t& version, Match& m);
