#pragma once
// St.-Pauli-Ticker: the team's (MATCH_TEAM_ID) match from OpenLigaDB
// (src/match_client.cpp), its score in the 4th row while it runs and as a
// promo for a while after (src/render.cpp), and a goal animation
// (src/goal_render.cpp). No Arduino dependency, unit-tested on the host.
#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct Match {
  uint32_t id = 0;        // OpenLigaDB matchID, 0 = none
  time_t kickoff = 0;     // UTC
  char home[24] = "";     // short names, "St. Pauli"
  char away[24] = "";
  char homeCode[8] = "";  // MATCH_TEAM_ABBREVIATIONS, "" if none
  char awayCode[8] = "";
  bool ourHome = false;   // MATCH_TEAM_ID is the home side
  uint8_t goalsHome = 0, goalsAway = 0;
  bool finished = false;
  char lastGoal[40] = ""; // "48' A. Ceesay", "" before the first
};

enum class MatchPhase : uint8_t { None, Next, Live, Final };

// Only the fields used below, for deserializeJson: of every match of a list
// (getmatchesbyteamid) or of one (getmatchdata/{id}).
void buildMatchFilter(JsonDocument& filter, bool list);

// One match object into `m`. false if it isn't one or `teamId` doesn't play.
// The score is that of the latest goal, or of the latest result if that is
// further (the goals may lag behind).
bool matchFromJson(JsonVariantConst o, int teamId, Match& m);

// The match to show from a list: a running one, else one over for less
// than MATCH_FINAL_H hours, else the next one; only leagues in `leagues`
// (CSV of leagueShortcut; OpenLigaDB has some matches twice, in a second
// league with another kick-off time). `m` is empty (id 0) if there is none.
// false only if `in` isn't an array.
bool matchesFromJson(JsonVariantConst in, int teamId, const char* leagues, time_t now, Match& m);

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

// The code for a team's short name from `table` ("name|code;..."), "" if none.
void teamCode(const char* table, const char* name, char* out, size_t outSize);

// "Hannover - St. Pauli", "Hannover 1:1 St. Pauli", "1:1"; with `codes`
// "H96 - STP" (names without a code stay).
void matchTeamsText(const Match& m, bool codes, char* out, size_t outSize);
void matchLineText(const Match& m, bool codes, char* out, size_t outSize);
void matchScoreText(const Match& m, char* out, size_t outSize);
