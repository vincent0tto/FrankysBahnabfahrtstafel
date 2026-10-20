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

static JsonDocument doc;
static Match m, before;

static time_t iso(const char* s) {
  time_t t = 0;
  parseIsoUtc(s, t);
  return t;
}

// getmatchesbyteamid/98/1/1 of 21.10.2026: Hannover - St. Pauli (bl2, 1:1,
// and the same once more as "bl2h" without a result), Darmstadt - St. Pauli
// (bl2 Fri 18:30, "bl2h" Sat 15:30)
static std::string readFixture() {
  for (const char* p : {"test/fixtures/stpauli.json", "../test/fixtures/stpauli.json"}) {
    std::ifstream f(p);
    if (f) { std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
  }
  TEST_FAIL_MESSAGE("fixture not found");
  return "";
}

static void loadFixture() {
  static std::string json = readFixture();
  JsonDocument filter;
  buildMatchFilter(filter, true);
  doc.clear();
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  TEST_ASSERT_FALSE_MESSAGE(err, err.c_str());
}

void setUp() {
  setenv("TZ", TZ_INFO, 1);
  tzset();
  loadFixture();
}
void tearDown() {}

void test_finished_match() {
  TEST_ASSERT_TRUE(matchFromJson(doc[1], MATCH_TEAM_ID, m));
  TEST_ASSERT_EQUAL(83627, m.id);
  TEST_ASSERT_EQUAL(iso("2026-10-17T18:30:00Z"), m.kickoff);
  TEST_ASSERT_EQUAL_STRING("Hannover", m.home);
  TEST_ASSERT_EQUAL_STRING("St. Pauli", m.away);
  TEST_ASSERT_FALSE(m.ourHome);
  TEST_ASSERT_TRUE(m.finished);
  TEST_ASSERT_EQUAL(1, m.goalsHome);
  TEST_ASSERT_EQUAL(1, m.goalsAway);
  TEST_ASSERT_EQUAL(1, ourGoals(m));
  TEST_ASSERT_EQUAL_STRING("48' Husseyn Chakroun", m.lastGoal);
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
}

void test_team_codes() {
  char c[8];
  teamCode(MATCH_TEAM_ABBREVIATIONS, "St. Pauli", c, sizeof(c));
  TEST_ASSERT_EQUAL_STRING("STP", c);
  teamCode(MATCH_TEAM_ABBREVIATIONS, "Fürth", c, sizeof(c));
  TEST_ASSERT_EQUAL_STRING("SGF", c);
  teamCode(MATCH_TEAM_ABBREVIATIONS, "Wolfsburg", c, sizeof(c));  // the last one
  TEST_ASSERT_EQUAL_STRING("WOB", c);
  teamCode(MATCH_TEAM_ABBREVIATIONS, "St.", c, sizeof(c));
  TEST_ASSERT_EQUAL_STRING("", c);
  teamCode(MATCH_TEAM_ABBREVIATIONS, "Schalke", c, sizeof(c));
  TEST_ASSERT_EQUAL_STRING("", c);
}

void test_other_team() { TEST_ASSERT_FALSE(matchFromJson(doc[1], 7, m)); }

void test_result_without_goals() {
  doc[1]["goals"].to<JsonArray>();  // the goals lag behind the result
  TEST_ASSERT_TRUE(matchFromJson(doc[1], MATCH_TEAM_ID, m));
  TEST_ASSERT_EQUAL(1, m.goalsHome);
  TEST_ASSERT_EQUAL(1, m.goalsAway);
  TEST_ASSERT_EQUAL_STRING("", m.lastGoal);
}

void test_live_before_any_goal() {
  doc[2]["matchIsFinished"] = false;
  TEST_ASSERT_TRUE(matchFromJson(doc[2], MATCH_TEAM_ID, m));
  TEST_ASSERT_EQUAL(0, m.goalsHome + m.goalsAway);
  TEST_ASSERT_EQUAL(MatchPhase::Live, matchPhase(m, m.kickoff + 60));
}

void test_phases() {
  TEST_ASSERT_TRUE(matchFromJson(doc[2], MATCH_TEAM_ID, m));  // Darmstadt, not finished
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

void test_list_final_then_next() {
  // Saturday night: Hannover is over, the bl2h copy without a result is left out
  TEST_ASSERT_TRUE(matchesFromJson(doc.as<JsonVariantConst>(), MATCH_TEAM_ID, MATCH_LEAGUES,
                                   iso("2026-10-17T21:00:00Z"), m));
  TEST_ASSERT_EQUAL(83627, m.id);
  // Wednesday: the next one, the bl2 kick-off (Friday), not the bl2h one (Saturday)
  TEST_ASSERT_TRUE(matchesFromJson(doc.as<JsonVariantConst>(), MATCH_TEAM_ID, MATCH_LEAGUES,
                                   iso("2026-10-21T12:00:00Z"), m));
  TEST_ASSERT_EQUAL(83648, m.id);
  TEST_ASSERT_EQUAL(iso("2026-10-23T16:30:00Z"), m.kickoff);
  TEST_ASSERT_EQUAL_STRING("Darmstadt", m.home);
}

void test_list_live_first() {
  doc[1]["matchIsFinished"] = false;
  // live beats the next one
  TEST_ASSERT_TRUE(matchesFromJson(doc.as<JsonVariantConst>(), MATCH_TEAM_ID, MATCH_LEAGUES,
                                   iso("2026-10-17T19:30:00Z"), m));
  TEST_ASSERT_EQUAL(83627, m.id);
  TEST_ASSERT_TRUE(matchPollLive(m, iso("2026-10-17T19:30:00Z")));
}

void test_list_leagues() {
  TEST_ASSERT_TRUE(matchesFromJson(doc.as<JsonVariantConst>(), MATCH_TEAM_ID, "bl2h", iso("2026-10-21T12:00:00Z"), m));
  TEST_ASSERT_EQUAL(83578, m.id);
  TEST_ASSERT_TRUE(matchesFromJson(doc.as<JsonVariantConst>(), MATCH_TEAM_ID, "bl1", iso("2026-10-21T12:00:00Z"), m));
  TEST_ASSERT_EQUAL(0, m.id);
  TEST_ASSERT_FALSE(matchesFromJson(doc[0], MATCH_TEAM_ID, MATCH_LEAGUES, 0, m));
}

void test_new_goals() {
  TEST_ASSERT_TRUE(matchFromJson(doc[1], MATCH_TEAM_ID, m));  // St. Pauli away, 1:1
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
  RUN_TEST(test_team_codes);
  RUN_TEST(test_other_team);
  RUN_TEST(test_result_without_goals);
  RUN_TEST(test_live_before_any_goal);
  RUN_TEST(test_phases);
  RUN_TEST(test_list_final_then_next);
  RUN_TEST(test_list_live_first);
  RUN_TEST(test_list_leagues);
  RUN_TEST(test_new_goals);
  return UNITY_END();
}
