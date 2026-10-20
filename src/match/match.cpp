#include "match/match.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "departures/parse.h"
#include "event/event.h"

void buildMatchFilter(JsonDocument& filter, bool list) {
  JsonObject o = list ? filter.to<JsonArray>().add<JsonObject>() : filter.to<JsonObject>();
  o["matchID"] = true;
  o["matchDateTimeUTC"] = true;
  o["leagueShortcut"] = true;
  o["matchIsFinished"] = true;
  for (const char* t : {"team1", "team2"}) {
    JsonObject team = o[t].to<JsonObject>();
    team["teamId"] = true;
    team["shortName"] = true;
    team["teamName"] = true;
  }
  JsonObject r = o["matchResults"].to<JsonArray>().add<JsonObject>();
  r["pointsTeam1"] = true;
  r["pointsTeam2"] = true;
  r["resultOrderID"] = true;
  JsonObject g = o["goals"].to<JsonArray>().add<JsonObject>();
  g["scoreTeam1"] = true;
  g["scoreTeam2"] = true;
  g["matchMinute"] = true;
  g["goalGetterName"] = true;
}

static void teamName(JsonVariantConst team, char* out, size_t outSize) {
  const char* s = team["shortName"];
  copyText(out, outSize, s && s[0] ? s : (const char*)(team["teamName"] | "?"));
}

bool matchFromJson(JsonVariantConst o, int teamId, Match& m) {
  m = Match();
  if (!o.is<JsonObjectConst>()) return false;
  bool home = o["team1"]["teamId"] == teamId;
  if (!home && o["team2"]["teamId"] != teamId) return false;
  m.id = o["matchID"] | 0u;
  if (!m.id || !parseIsoUtc(o["matchDateTimeUTC"], m.kickoff)) return false;
  m.ourHome = home;
  teamName(o["team1"], m.home, sizeof(m.home));
  teamName(o["team2"], m.away, sizeof(m.away));
  teamCode(MATCH_TEAM_ABBREVIATIONS, m.home, m.homeCode, sizeof(m.homeCode));
  teamCode(MATCH_TEAM_ABBREVIATIONS, m.away, m.awayCode, sizeof(m.awayCode));
  m.finished = o["matchIsFinished"] | false;

  // the latest goal: the most goals, on a tie the later one in the list
  int best = -1;
  for (JsonVariantConst g : o["goals"].as<JsonArrayConst>()) {
    int h = g["scoreTeam1"] | 0, a = g["scoreTeam2"] | 0;
    if (h + a < best) continue;
    best = h + a;
    m.goalsHome = (uint8_t)h;
    m.goalsAway = (uint8_t)a;
    int minute = g["matchMinute"] | 0;
    char t[64];
    const char* name = g["goalGetterName"] | "";
    if (minute > 0) snprintf(t, sizeof(t), "%d' %s", minute, name);
    else snprintf(t, sizeof(t), "%s", name);
    copyText(m.lastGoal, sizeof(m.lastGoal), t);
  }
  // the latest result (half-time, end) if it is further
  int order = -1;
  JsonVariantConst latest;
  for (JsonVariantConst r : o["matchResults"].as<JsonArrayConst>()) {
    if ((r["resultOrderID"] | 0) <= order) continue;
    order = r["resultOrderID"] | 0;
    latest = r;
  }
  if (!latest.isNull()) {
    int h = latest["pointsTeam1"] | 0, a = latest["pointsTeam2"] | 0;
    if (h + a > best) {
      m.goalsHome = (uint8_t)h;
      m.goalsAway = (uint8_t)a;
      m.lastGoal[0] = 0;  // the goals don't tell who
    }
  }
  return true;
}

bool matchesFromJson(JsonVariantConst in, int teamId, const char* leagues, time_t now, Match& m) {
  m = Match();
  if (!in.is<JsonArrayConst>()) return false;
  Match c;
  int bestRank = 0;  // 3 live, 2 final, 1 next
  for (JsonVariantConst o : in.as<JsonArrayConst>()) {
    if (!csvContains(leagues, o["leagueShortcut"] | "") || !matchFromJson(o, teamId, c)) continue;
    MatchPhase p = matchPhase(c, now);
    int rank = p == MatchPhase::Live ? 3 : p == MatchPhase::Final ? 2 : p == MatchPhase::Next ? 1 : 0;
    // of two finals the later, of two next ones the earlier
    bool better = rank > bestRank || (rank == bestRank && rank > 0 &&
                                      (rank == 1 ? c.kickoff < m.kickoff : c.kickoff > m.kickoff));
    if (better) {
      m = c;
      bestRank = rank;
    }
  }
  return true;
}

MatchPhase matchPhase(const Match& m, time_t now) {
  if (!m.id) return MatchPhase::None;
  if (now < m.kickoff) return MatchPhase::Next;
  // the end isn't known, so the final score counts from a guessed one
  time_t end = m.kickoff + MATCH_LIVE_MAX_S;
  if (!m.finished && now < end) return MatchPhase::Live;
  if (now < m.kickoff + 2 * 3600 + MATCH_FINAL_H * 3600L) return MatchPhase::Final;
  return MatchPhase::None;
}

bool matchPollLive(const Match& m, time_t now) {
  return m.id && !m.finished && now >= m.kickoff - MATCH_LIVE_LEAD_S && now < m.kickoff + MATCH_LIVE_MAX_S;
}

int ourGoals(const Match& m) { return m.ourHome ? m.goalsHome : m.goalsAway; }

int newGoals(const Match& before, const Match& after) {
  if (!before.id || before.id != after.id) return 0;
  int d = ourGoals(after) - ourGoals(before);
  return d > 0 ? d : 0;
}

void teamCode(const char* table, const char* name, char* out, size_t outSize) {
  out[0] = 0;
  size_t len = strlen(name);
  for (const char* p = table; *p;) {
    const char* bar = strchr(p, '|');
    if (!bar) return;
    const char* end = strchr(bar + 1, ';');
    size_t n = end ? (size_t)(end - bar - 1) : strlen(bar + 1);
    if ((size_t)(bar - p) == len && strncmp(p, name, len) == 0) {
      snprintf(out, outSize, "%.*s", (int)n, bar + 1);
      return;
    }
    if (!end) return;
    p = end + 1;
  }
}

static const char* homeName(const Match& m, bool codes) { return codes && m.homeCode[0] ? m.homeCode : m.home; }
static const char* awayName(const Match& m, bool codes) { return codes && m.awayCode[0] ? m.awayCode : m.away; }

void matchTeamsText(const Match& m, bool codes, char* out, size_t outSize) {
  snprintf(out, outSize, "%s - %s", homeName(m, codes), awayName(m, codes));
}

void matchLineText(const Match& m, bool codes, char* out, size_t outSize) {
  snprintf(out, outSize, "%s %d:%d %s", homeName(m, codes), m.goalsHome, m.goalsAway, awayName(m, codes));
}

void matchScoreText(const Match& m, char* out, size_t outSize) {
  snprintf(out, outSize, "%d:%d", m.goalsHome, m.goalsAway);
}
