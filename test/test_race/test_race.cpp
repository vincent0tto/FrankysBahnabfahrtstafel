// Host tests: pio test -e native
#include <unity.h>

#include "race.h"

void setUp() {}
void tearDown() {}

static Race r;

// Replays the moves: never behind the start or past the finish, only the
// winner reaches the finish, and only with the last move.
static void checkRace(uint32_t seed) {
  raceBegin(r, seed);
  TEST_ASSERT_TRUE(r.nMoves > 0 && r.nMoves <= RACE_MAX_MOVES);
  int pos[RACE_LANES] = {};
  uint32_t prev = 0;
  for (int i = 0; i < r.nMoves; i++) {
    const RaceMove& m = r.moves[i];
    TEST_ASSERT_TRUE(m.lane < RACE_LANES);
    TEST_ASSERT_TRUE(m.delta >= -1 && m.delta <= 2 && m.delta != 0);
    TEST_ASSERT_TRUE(m.at10ms > prev);
    prev = m.at10ms;
    pos[m.lane] += m.delta;
    TEST_ASSERT_TRUE(pos[m.lane] >= 0 && pos[m.lane] <= RACE_FIELDS);
    TEST_ASSERT_EQUAL(i == r.nMoves - 1, pos[m.lane] == RACE_FIELDS);
  }
  TEST_ASSERT_EQUAL(RACE_FIELDS, pos[r.winner]);
  TEST_ASSERT_EQUAL(r.moves[r.nMoves - 1].at10ms * 10u + RACE_SLIDE_MS, r.endMs);
}

void test_races_are_valid() {
  for (uint32_t s = 0; s < 10000; s++) checkRace(s * 2654435761u);
}

void test_same_seed_same_race() {
  raceBegin(r, 42);
  int winner = r.winner, n = r.nMoves;
  uint32_t end = r.endMs;
  raceBegin(r, 7);
  raceBegin(r, 42);
  TEST_ASSERT_EQUAL(winner, r.winner);
  TEST_ASSERT_EQUAL(n, r.nMoves);
  TEST_ASSERT_EQUAL(end, r.endMs);
}

// Every number has the same chance (people bet on it), ~30 s on average.
void test_fair_and_about_30s() {
  const int N = 10000;
  int wins[RACE_LANES] = {};
  double sum = 0;
  for (uint32_t s = 1; s <= N; s++) {
    raceBegin(r, s);
    wins[r.winner]++;
    sum += r.endMs;
  }
  for (int i = 0; i < RACE_LANES; i++) TEST_ASSERT_INT_WITHIN(N / 50, N / 4, wins[i]);
  TEST_ASSERT_INT_WITHIN(4000, 30000, (int)(sum / N));
}

void test_positions_glide_and_settle() {
  raceBegin(r, 99);
  int32_t pos[RACE_LANES];
  racePositions(r, 0, pos);
  for (int i = 0; i < RACE_LANES; i++) TEST_ASSERT_EQUAL(0, pos[i]);
  racePositions(r, r.endMs, pos);
  TEST_ASSERT_EQUAL(RACE_FIELDS * 256, pos[r.winner]);
  // half-way through the first move it is between its fields
  const RaceMove& m = r.moves[0];
  racePositions(r, m.at10ms * 10u + RACE_SLIDE_MS / 2, pos);
  TEST_ASSERT_EQUAL(m.delta * 128, pos[m.lane]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_races_are_valid);
  RUN_TEST(test_same_seed_same_race);
  RUN_TEST(test_fair_and_about_30s);
  RUN_TEST(test_positions_glide_and_settle);
  return UNITY_END();
}
