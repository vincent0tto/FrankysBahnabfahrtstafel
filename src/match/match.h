#pragma once
// St.-Pauli-Ticker: the team's (MATCH_TEAM_ID) match from ESPN
// (src/match/match_client.cpp), its score in the 4th row while it runs and as a
// promo for a while after (src/display/render.cpp), and a goal animation
// (src/match/goal_render.cpp). No Arduino dependency, unit-tested on the host.
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct Match {
  uint32_t id = 0;        // ESPN event id, 0 = none
  char league[16] = "";   // ESPN league slug, "ger.2"; the scoreboard to ask while live
  time_t kickoff = 0;     // UTC
  char home[24] = "";     // short names, "St. Pauli"
  char away[24] = "";
  char homeCode[8] = "";  // ESPN abbreviations, "STP"
  char awayCode[8] = "";
  bool ourHome = false;   // MATCH_TEAM_ID is the home side
  uint8_t goalsHome = 0, goalsAway = 0;
  bool finished = false;
  char lastGoal[40] = ""; // "48' H. Chakroun", "" before the first
};

enum class MatchPhase : uint8_t { None, Next, Live, Final };

// Only the fields used below, for deserializeJson: of the events of a team
// schedule (`scoreboard` false) or of a league's scoreboard (with the goals).
void buildMatchFilter(JsonDocument& filter, bool scoreboard);

// One event object into `m`. false if it isn't one, `teamId` doesn't play or
// it was called off. `league` is the scoreboard's (its events don't say), or
// nullptr for a schedule's event. The last goal only if the goals listed add
// up to the score (they may lag behind).
bool matchFromJson(JsonVariantConst ev, const char* teamId, const char* league, Match& m);

// The match to show from a team schedule's events: a running one, else one
// over for less than MATCH_FINAL_H hours, else the next one; only leagues in
// `leagues` (CSV of ESPN slugs). Compares with what `m` holds already, so
// the results and the fixtures can go in one after the other; start with an
// empty `m`. false only if there are no events.
bool matchesFromJson(JsonVariantConst schedule, const char* teamId, const char* leagues, time_t now, Match& m);

// The match `id` from a league's scoreboard into `m`. false if it isn't there.
bool matchFromScoreboard(JsonVariantConst scoreboard, uint32_t id, const char* teamId, Match& m);

// Live from kick-off until it is finished (at most MATCH_LIVE_MAX_S), then
// Final for MATCH_FINAL_H hours, Next before kick-off.
MatchPhase matchPhase(const Match& m, time_t now);

// Whether to ask for the score every MATCH_POLL_LIVE_S: from shortly before
// kick-off until it is finished.
bool matchPollLive(const Match& m, time_t now);

int ourGoals(const Match& m);

// Goals MATCH_TEAM_ID scored from `before` to `after`: only for the same
// match, never on the first look at it, 0 if the score went down (VAR).
int newGoals(const Match& before, const Match& after);

// "Hannover - St. Pauli", "Hannover 1:1 St. Pauli", "1:1"; with `codes`
// "H96 - STP" (names without a code stay).
void matchTeamsText(const Match& m, bool codes, char* out, size_t outSize);
void matchLineText(const Match& m, bool codes, char* out, size_t outSize);
void matchScoreText(const Match& m, char* out, size_t outSize);
