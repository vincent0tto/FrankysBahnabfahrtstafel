#include "match/match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "departures/parse.h"
#include "event/event.h"

void buildMatchFilter(JsonDocument& filter, bool scoreboard) {
  if (scoreboard) filter["leagues"][0]["slug"] = true;
  JsonObject e = filter["events"].to<JsonArray>().add<JsonObject>();
  e["id"] = true;
  e["date"] = true;
  if (!scoreboard) e["league"]["slug"] = true;
  JsonObject c = e["competitions"].to<JsonArray>().add<JsonObject>();
  c["status"]["type"]["state"] = true;
  c["status"]["type"]["completed"] = true;
  JsonObject t = c["competitors"].to<JsonArray>().add<JsonObject>();
  t["homeAway"] = true;
  t["team"]["id"] = true;
  t["team"]["shortDisplayName"] = true;
  t["team"]["abbreviation"] = true;
  t["score"] = true;  // "1" on a scoreboard, {"value":1.0,...} in a schedule
  if (!scoreboard) return;
  JsonObject d = c["details"].to<JsonArray>().add<JsonObject>();
  d["scoringPlay"] = true;
  d["shootout"] = true;
  d["ownGoal"] = true;
  d["clock"]["displayValue"] = true;
  d["athletesInvolved"][0]["shortName"] = true;
}

static int score(JsonVariantConst s) {
  if (s.is<const char*>()) return atoi(s.as<const char*>());
  return (int)(s["value"] | 0.0);
}

bool matchFromJson(JsonVariantConst ev, const char* teamId, const char* league, Match& m) {
  m = Match();
  if (!ev.is<JsonObjectConst>()) return false;
  JsonVariantConst c = ev["competitions"][0];
  JsonVariantConst home, away;
  for (JsonVariantConst t : c["competitors"].as<JsonArrayConst>())
    (strcmp(t["homeAway"] | "", "home") == 0 ? home : away) = t;
  if (home.isNull() || away.isNull()) return false;
  m.ourHome = strcmp(home["team"]["id"] | "", teamId) == 0;
  if (!m.ourHome && strcmp(away["team"]["id"] | "", teamId) != 0) return false;
  m.id = (uint32_t)strtoul(ev["id"] | "0", nullptr, 10);
  if (!m.id || !parseIsoUtc(ev["date"], m.kickoff)) return false;

  JsonVariantConst type = c["status"]["type"];
  m.finished = type["completed"] | false;
  // postponed, cancelled: over without being played
  if (!m.finished && strcmp(type["state"] | "", "post") == 0) return false;

  copyText(m.league, sizeof(m.league), league ? league : (const char*)(ev["league"]["slug"] | ""));
  copyText(m.home, sizeof(m.home), home["team"]["shortDisplayName"] | "?");
  copyText(m.away, sizeof(m.away), away["team"]["shortDisplayName"] | "?");
  copyText(m.homeCode, sizeof(m.homeCode), home["team"]["abbreviation"] | "");
  copyText(m.awayCode, sizeof(m.awayCode), away["team"]["abbreviation"] | "");
  m.goalsHome = (uint8_t)score(home["score"]);
  m.goalsAway = (uint8_t)score(away["score"]);

  // the last goal, if the goals listed are all there are (not the shoot-out)
  int goals = 0;
  JsonVariantConst last;
  for (JsonVariantConst d : c["details"].as<JsonArrayConst>()) {
    if (!(d["scoringPlay"] | false) || (d["shootout"] | false)) continue;
    goals++;
    last = d;
  }
  if (goals && goals == m.goalsHome + m.goalsAway) {
    char t[64];
    snprintf(t, sizeof(t), "%s %s%s", last["clock"]["displayValue"] | "", last["athletesInvolved"][0]["shortName"] | "?",
             (last["ownGoal"] | false) ? " (ET)" : "");
    copyText(m.lastGoal, sizeof(m.lastGoal), t);
  }
  return true;
}

bool matchesFromJson(JsonVariantConst schedule, const char* teamId, const char* leagues, time_t now, Match& m) {
  JsonArrayConst events = schedule["events"];
  if (events.isNull()) return false;
  auto rank = [now](const Match& x) {  // 3 live, 2 final, 1 next
    MatchPhase p = matchPhase(x, now);
    return p == MatchPhase::Live ? 3 : p == MatchPhase::Final ? 2 : p == MatchPhase::Next ? 1 : 0;
  };
  Match c;
  int bestRank = rank(m);
  for (JsonVariantConst ev : events) {
    if (!csvContains(leagues, ev["league"]["slug"] | "") || !matchFromJson(ev, teamId, nullptr, c)) continue;
    int r = rank(c);
    // of two finals the later, of two next ones the earlier
    bool better = r > bestRank || (r == bestRank && r > 0 &&
                                   (r == 1 ? c.kickoff < m.kickoff : c.kickoff > m.kickoff));
    if (better) {
      m = c;
      bestRank = r;
    }
  }
  return true;
}

bool matchFromScoreboard(JsonVariantConst scoreboard, uint32_t id, const char* teamId, Match& m) {
  const char* league = scoreboard["leagues"][0]["slug"] | "";
  for (JsonVariantConst ev : scoreboard["events"].as<JsonArrayConst>())
    if (strtoul(ev["id"] | "0", nullptr, 10) == id) return matchFromJson(ev, teamId, league, m);
  return false;
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
