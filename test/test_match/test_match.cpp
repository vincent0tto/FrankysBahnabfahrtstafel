// Host tests: pio test -e native
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <fstream>
#include <sstream>
#include <string>

#include "config.h"
#include "departures/parse.h"
#include "match/match.h"

static JsonDocument results, fixtures, scoreboard;
static Match m, before;

static time_t iso(const char* s) {
  time_t t = 0;
  parseIsoUtc(s, t);
  return t;
}

static std::string readFixture(const char* name) {
  for (const char* dir : {"test/fixtures/", "../test/fixtures/"}) {
    std::ifstream f(std::string(dir) + name);
    if (f) { std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
  }
  TEST_FAIL_MESSAGE(name);
  return "";
}

static void load(JsonDocument& doc, const char* name, bool isScoreboard) {
  JsonDocument filter;
  buildMatchFilter(filter, isScoreboard);
  doc.clear();
  DeserializationError err = deserializeJson(doc, readFixture(name), DeserializationOption::Filter(filter));
  TEST_ASSERT_FALSE_MESSAGE(err, err.c_str());
}

// Of 23.10.2026, links and logos taken out: all/teams/270/schedule (results:
// Hannover - St. Pauli 1:1 on 17.10., a friendly on 3.10., the cup),
// all/teams/270/schedule?fixture=true (Darmstadt - St. Pauli 23.10. 16:30 UTC
// first), ger.2/scoreboard?dates=20261017 (Hannover - St. Pauli with the goals).
void setUp() {
  setenv("TZ", TZ_INFO, 1);
  tzset();
  load(results, "espn_results.json", false);
  load(fixtures, "espn_fixtures.json", false);
  load(scoreboard, "espn_scoreboard.json", true);
}
void tearDown() {}

static JsonVariant hannoverOnBoard() {
  for (JsonVariant ev : scoreboard["events"].as<JsonArray>())
    if (strcmp(ev["id"] | "", "401885091") == 0) return ev;
  TEST_FAIL_MESSAGE("Hannover not on the scoreboard");
  return JsonVariant();
}

void test_finished_match() {
  TEST_ASSERT_TRUE(matchFromJson(results["events"][0], MATCH_TEAM_ID, nullptr, m));
  TEST_ASSERT_EQUAL(401885091, m.id);
  TEST_ASSERT_EQUAL_STRING("ger.2", m.league);
  TEST_ASSERT_EQUAL(iso("2026-10-17T18:30:00Z"), m.kickoff);
  TEST_ASSERT_EQUAL_STRING("Hannover", m.home);
  TEST_ASSERT_EQUAL_STRING("St. Pauli", m.away);
  TEST_ASSERT_EQUAL_STRING("H96", m.homeCode);
  TEST_ASSERT_EQUAL_STRING("STP", m.awayCode);
  TEST_ASSERT_FALSE(m.ourHome);
  TEST_ASSERT_TRUE(m.finished);
  TEST_ASSERT_EQUAL(1, m.goalsHome);
  TEST_ASSERT_EQUAL(1, m.goalsAway);
  TEST_ASSERT_EQUAL(1, ourGoals(m));
  TEST_ASSERT_EQUAL_STRING("", m.lastGoal);  // a schedule has no goals
  char t[48];
  matchTeamsText(m, false, t, sizeof(t));
  TEST_ASSERT_EQUAL_STRING("Hannover - St. Pauli", t);
  matchTeamsText(m, true, t, sizeof(t));
  TEST_ASSERT_EQUAL_STRING("H96 - STP", t);
  matchLineText(m, false, t, sizeof(t));
  TEST_ASSERT_EQUAL_STRING("Hannover 1:1 St. Pauli", t);
  strcpy(m.homeCode, "");  // a team without a code keeps its name
  matchLineText(m, true, t, sizeof(t));
  TEST_ASSERT_EQUAL_STRING("Hannover 1:1 STP", t);
  matchScoreText(m, t, sizeof(t));
  TEST_ASSERT_EQUAL_STRING("1:1", t);
  TEST_ASSERT_EQUAL_STRING("Hannover", matchHomeName(m, true));
  TEST_ASSERT_EQUAL_STRING("St. Pauli", matchAwayName(m, false));
  TEST_ASSERT_EQUAL_STRING("STP", matchAwayName(m, true));
}

void test_scoreboard_goals() {
  TEST_ASSERT_TRUE(matchFromScoreboard(scoreboard.as<JsonVariantConst>(), 401885091, MATCH_TEAM_ID, m));
  TEST_ASSERT_EQUAL_STRING("ger.2", m.league);
  TEST_ASSERT_EQUAL(1, m.goalsHome);
  TEST_ASSERT_EQUAL(1, m.goalsAway);
  TEST_ASSERT_TRUE(m.finished);
  TEST_ASSERT_EQUAL_STRING("48' H. Chakroun", m.lastGoal);
  TEST_ASSERT_FALSE(matchFromScoreboard(scoreboard.as<JsonVariantConst>(), 401885082, MATCH_TEAM_ID, m));
}

void test_goals_lag_behind() {
  JsonVariant ev = hannoverOnBoard();
  ev["competitions"][0]["competitors"][1]["score"] = "2";  // St. Pauli's second, not listed yet
  TEST_ASSERT_TRUE(matchFromJson(ev, MATCH_TEAM_ID, "ger.2", m));
  TEST_ASSERT_EQUAL(2, m.goalsAway);
  TEST_ASSERT_EQUAL_STRING("", m.lastGoal);
}

void test_clock() {
  JsonVariant ev = hannoverOnBoard();
  JsonVariant status = ev["competitions"][0]["status"];
  TEST_ASSERT_TRUE(matchFromJson(ev, MATCH_TEAM_ID, "ger.2", m));
  TEST_ASSERT_EQUAL_STRING("", m.clock);  // finished: no minute
  status["type"]["state"] = "in";
  status["type"]["completed"] = false;
  status["type"]["name"] = "STATUS_SECOND_HALF";
  status["displayClock"] = "67'";
  TEST_ASSERT_TRUE(matchFromJson(ev, MATCH_TEAM_ID, "ger.2", m));
  TEST_ASSERT_EQUAL_STRING("67'", m.clock);
  status["displayClock"] = "90'+4'";
  TEST_ASSERT_TRUE(matchFromJson(ev, MATCH_TEAM_ID, "ger.2", m));
  TEST_ASSERT_EQUAL_STRING("90+4'", m.clock);
  status["type"]["name"] = "STATUS_HALFTIME";
  status["displayClock"] = "45'+2'";
  TEST_ASSERT_TRUE(matchFromJson(ev, MATCH_TEAM_ID, "ger.2", m));
  TEST_ASSERT_EQUAL_STRING("HZ", m.clock);
}

void test_scheduled_match() {
  TEST_ASSERT_TRUE(matchFromJson(fixtures["events"][0], MATCH_TEAM_ID, nullptr, m));
  TEST_ASSERT_EQUAL(401885082, m.id);
  TEST_ASSERT_EQUAL(iso("2026-10-23T16:30:00Z"), m.kickoff);
  TEST_ASSERT_EQUAL_STRING("Darmstadt", m.home);
  TEST_ASSERT_EQUAL(0, m.goalsHome + m.goalsAway);  // no score yet
  TEST_ASSERT_FALSE(m.finished);
}

void test_other_team() { TEST_ASSERT_FALSE(matchFromJson(results["events"][0], "7", nullptr, m)); }

void test_postponed() {
  JsonVariant type = fixtures["events"][0]["competitions"][0]["status"]["type"];
  type["state"] = "post";  // STATUS_POSTPONED: over, not completed
  TEST_ASSERT_FALSE(matchFromJson(fixtures["events"][0], MATCH_TEAM_ID, nullptr, m));
}

void test_phases() {
  TEST_ASSERT_TRUE(matchFromJson(fixtures["events"][0], MATCH_TEAM_ID, nullptr, m));  // Darmstadt
  TEST_ASSERT_EQUAL(MatchPhase::Next, matchPhase(m, m.kickoff - 1));
  TEST_ASSERT_FALSE(matchPollLive(m, m.kickoff - MATCH_LIVE_LEAD_S - 1));
  TEST_ASSERT_TRUE(matchPollLive(m, m.kickoff - MATCH_LIVE_LEAD_S));
  TEST_ASSERT_EQUAL(MatchPhase::Live, matchPhase(m, m.kickoff));
  TEST_ASSERT_EQUAL(MatchPhase::Live, matchPhase(m, m.kickoff + 100 * 60));
  // never marked as finished: not live for ever
  TEST_ASSERT_EQUAL(MatchPhase::Final, matchPhase(m, m.kickoff + MATCH_LIVE_MAX_S));
  TEST_ASSERT_FALSE(matchPollLive(m, m.kickoff + MATCH_LIVE_MAX_S));
  m.finished = true;
  TEST_ASSERT_FALSE(matchPollLive(m, m.kickoff + 100 * 60));
  TEST_ASSERT_EQUAL(MatchPhase::Final, matchPhase(m, m.kickoff + 100 * 60));
  TEST_ASSERT_EQUAL(MatchPhase::Final, matchPhase(m, m.kickoff + (2 + MATCH_FINAL_H) * 3600 - 1));
  TEST_ASSERT_EQUAL(MatchPhase::None, matchPhase(m, m.kickoff + (2 + MATCH_FINAL_H) * 3600));
  TEST_ASSERT_EQUAL(MatchPhase::None, matchPhase(Match(), 0));
}

// The results, then the fixtures, as the board asks for them.
static void list(time_t now, const char* leagues = MATCH_LEAGUES) {
  m = Match();
  TEST_ASSERT_TRUE(matchesFromJson(results.as<JsonVariantConst>(), MATCH_TEAM_ID, leagues, now, m));
  TEST_ASSERT_TRUE(matchesFromJson(fixtures.as<JsonVariantConst>(), MATCH_TEAM_ID, leagues, now, m));
}

void test_list_final_then_next() {
  list(iso("2026-10-17T21:00:00Z"));  // Saturday night: Hannover is over
  TEST_ASSERT_EQUAL(401885091, m.id);
  list(iso("2026-10-21T12:00:00Z"));  // Wednesday: the next one, not a later one
  TEST_ASSERT_EQUAL(401885082, m.id);
  TEST_ASSERT_EQUAL_STRING("Darmstadt", m.home);
}

void test_list_live_first() {
  JsonVariant type = results["events"][0]["competitions"][0]["status"]["type"];
  type["state"] = "in";
  type["completed"] = false;
  list(iso("2026-10-17T19:30:00Z"));  // live beats the next one
  TEST_ASSERT_EQUAL(401885091, m.id);
  TEST_ASSERT_TRUE(matchPollLive(m, iso("2026-10-17T19:30:00Z")));
}

void test_list_keeps_what_it_has() {
  // a running match neither schedule lists (yet) stays
  TEST_ASSERT_TRUE(matchFromScoreboard(scoreboard.as<JsonVariantConst>(), 401885091, MATCH_TEAM_ID, m));
  m.finished = false;
  results["events"].as<JsonArray>().remove(0);
  TEST_ASSERT_TRUE(matchesFromJson(results.as<JsonVariantConst>(), MATCH_TEAM_ID, MATCH_LEAGUES,
                                   iso("2026-10-17T19:30:00Z"), m));
  TEST_ASSERT_TRUE(matchesFromJson(fixtures.as<JsonVariantConst>(), MATCH_TEAM_ID, MATCH_LEAGUES,
                                   iso("2026-10-17T19:30:00Z"), m));
  TEST_ASSERT_EQUAL(401885091, m.id);
  TEST_ASSERT_EQUAL_STRING("48' H. Chakroun", m.lastGoal);
}

void test_list_leagues() {
  list(iso("2026-10-03T15:00:00Z"));  // the friendly on 3.10. doesn't count: the next one
  TEST_ASSERT_EQUAL(401885101, m.id);  // Karlsruhe, 11.10.
  list(iso("2026-10-03T15:00:00Z"), "club.friendly");
  TEST_ASSERT_EQUAL(401920464, m.id);
  list(iso("2026-10-21T12:00:00Z"), "ger.1");
  TEST_ASSERT_EQUAL(0, m.id);
  TEST_ASSERT_FALSE(matchesFromJson(results["events"][0], MATCH_TEAM_ID, MATCH_LEAGUES, 0, m));
}

void test_new_goals() {
  TEST_ASSERT_TRUE(matchFromJson(results["events"][0], MATCH_TEAM_ID, nullptr, m));  // St. Pauli away, 1:1
  before = m;
  before.goalsAway = 0;
  TEST_ASSERT_EQUAL(1, newGoals(before, m));
  TEST_ASSERT_EQUAL(0, newGoals(m, before));  // taken back
  TEST_ASSERT_EQUAL(0, newGoals(Match(), m)); // first look after the start
  before = m;
  m.goalsHome = 2;                            // the other side scored
  TEST_ASSERT_EQUAL(0, newGoals(before, m));
  before.id = 1;
  m.goalsAway = 3;
  TEST_ASSERT_EQUAL(0, newGoals(before, m));  // another match
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_finished_match);
  RUN_TEST(test_scoreboard_goals);
  RUN_TEST(test_goals_lag_behind);
  RUN_TEST(test_clock);
  RUN_TEST(test_scheduled_match);
  RUN_TEST(test_other_team);
  RUN_TEST(test_postponed);
  RUN_TEST(test_phases);
  RUN_TEST(test_list_final_then_next);
  RUN_TEST(test_list_live_first);
  RUN_TEST(test_list_keeps_what_it_has);
  RUN_TEST(test_list_leagues);
  RUN_TEST(test_new_goals);
  return UNITY_END();
}
