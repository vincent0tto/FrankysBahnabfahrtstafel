// Host tests: pio test -e native
#include <unity.h>

#include "race/race.h"

void setUp() {}
void tearDown() {}

static Race r;

// Replays the moves: never behind the start or past the finish, only the
// winner reaches the finish, and only with the last move; a derailed tram
// moves no more and the derailment pauses the race.
static void checkRace(uint32_t seed) {
  raceBegin(r, seed);
  TEST_ASSERT_TRUE(r.nMoves > 0 && r.nMoves <= RACE_MAX_MOVES);
  int pos[RACE_LANES] = {};
  uint32_t prev = 0;
  for (int i = 0; i < r.nMoves; i++) {
    const RaceMove& m = r.moves[i];
    TEST_ASSERT_TRUE(m.lane < RACE_LANES);
    if (m.lane == r.iceLane) {  // the ICE: one run from the start to the finish, as the last move
      TEST_ASSERT_FALSE(r.iceCancelled);
      TEST_ASSERT_EQUAL(RACE_FIELDS, m.delta);
      TEST_ASSERT_EQUAL(r.nMoves - 1, i);
    } else {
      TEST_ASSERT_TRUE(m.delta >= -1 && m.delta <= 2 && m.delta != 0);
    }
    TEST_ASSERT_TRUE(m.at10ms > prev);
    if (r.derailLane >= 0 && m.at10ms * 10u > r.derailMs) {
      TEST_ASSERT_NOT_EQUAL(r.derailLane, m.lane);
      if (prev * 10u <= r.derailMs) TEST_ASSERT_TRUE(m.at10ms * 10u >= r.derailMs + RACE_DERAIL_MS);
    }
    if (r.fiesta && m.at10ms * 10u > r.fiestaMs && prev * 10u <= r.fiestaMs)  // nobody moves while he dances
      TEST_ASSERT_TRUE(m.at10ms * 10u >= r.fiestaMs + RACE_FIESTA_MS);
    prev = m.at10ms;
    pos[m.lane] += m.delta;
    TEST_ASSERT_TRUE(pos[m.lane] >= 0 && pos[m.lane] <= RACE_FIELDS);
    TEST_ASSERT_EQUAL(i == r.nMoves - 1, pos[m.lane] == RACE_FIELDS);
  }
  TEST_ASSERT_EQUAL(RACE_FIELDS, pos[r.winner]);
  if (r.derailLane >= 0) TEST_ASSERT_TRUE(r.derailMs < r.endMs);
  if (r.fiesta) {
    TEST_ASSERT_EQUAL(-1, r.derailLane);
    TEST_ASSERT_TRUE(r.fiestaMs > 0 && r.fiestaMs < r.endMs);
  }
  if (r.iceLane >= 0) {
    TEST_ASSERT_EQUAL(-1, r.derailLane);
    TEST_ASSERT_EQUAL(!r.iceCancelled, r.winner == r.iceLane);
    if (!r.iceCancelled)  // set off once another was half-way
      for (int i = 0; i < RACE_LANES; i++) TEST_ASSERT_TRUE(i == r.iceLane || pos[i] <= RACE_FIELDS / 2 + 1);
  }

  int order[RACE_LANES], field[RACE_LANES];
  raceRanking(r, order, field);
  TEST_ASSERT_EQUAL(r.winner, order[0]);
  if (r.derailLane >= 0) TEST_ASSERT_EQUAL(r.derailLane, order[RACE_LANES - 1]);
  if (r.iceCancelled) TEST_ASSERT_EQUAL(r.iceLane, order[RACE_LANES - 1]);
  int seen = 0;
  for (int i = 0; i < RACE_LANES; i++) {
    seen |= 1 << order[i];
    TEST_ASSERT_EQUAL(pos[i], field[i]);
    if (i > 0 && order[i] != r.derailLane && !(r.iceCancelled && order[i] == r.iceLane)) TEST_ASSERT_TRUE(field[order[i - 1]] >= field[order[i]]);
  }
  TEST_ASSERT_EQUAL(0xF, seen);
  TEST_ASSERT_EQUAL(r.moves[r.nMoves - 1].at10ms * 10u + (r.winner == r.iceLane ? RACE_ICE_MS : RACE_SLIDE_MS),
                    r.endMs);
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

// Every number has the same chance (people bet on it), also to derail or
// be the ICE; ~30 s on average, a derailment in about every 10th race, an
// ICE in every 12th, one in 3 of them cancelled, the fiesta in every 4th
// without a derailment.
void test_fair_and_about_30s() {
  const int N = 20000;
  int wins[RACE_LANES] = {}, derails[RACE_LANES] = {}, nDerail = 0, ices[RACE_LANES] = {}, nIce = 0, nCancelled = 0, nFiesta = 0;
  double sum = 0;
  for (uint32_t s = 1; s <= N; s++) {
    raceBegin(r, s);
    wins[r.winner]++;
    sum += r.endMs - (r.derailLane >= 0 ? RACE_DERAIL_MS : 0) - (r.fiesta ? RACE_FIESTA_MS : 0);
    nFiesta += r.fiesta;
    if (r.derailLane >= 0) derails[r.derailLane]++, nDerail++;
    if (r.iceLane >= 0) ices[r.iceLane]++, nIce++, nCancelled += r.iceCancelled;
  }
  for (int i = 0; i < RACE_LANES; i++) TEST_ASSERT_INT_WITHIN(N / 50, N / 4, wins[i]);
  for (int i = 0; i < RACE_LANES; i++) TEST_ASSERT_INT_WITHIN(nDerail / 8, nDerail / 4, derails[i]);
  TEST_ASSERT_INT_WITHIN(N / 50, N / 10, nDerail);
  for (int i = 0; i < RACE_LANES; i++) TEST_ASSERT_INT_WITHIN(nIce / 6, nIce / 4, ices[i]);
  TEST_ASSERT_INT_WITHIN(N / 100, N * 9 / 10 / 12, nIce);  // not with a derailment
  TEST_ASSERT_INT_WITHIN(nIce / 20, nIce / 3, nCancelled);
  TEST_ASSERT_INT_WITHIN(N / 50, N * 9 / 10 / 4, nFiesta);
  TEST_ASSERT_INT_WITHIN(4000, 30000, (int)(sum / N));
}

// The Mexican as often as set: never with 0, with 1 in every race without
// a derailment, otherwise in about 1 of N.
void test_fiesta_chance() {
  int n0 = 0, n1 = 0, nDerail = 0, n2 = 0;
  for (uint32_t s = 1; s <= 4000; s++) {
    raceBegin(r, s, 0);
    n0 += r.fiesta;
    raceBegin(r, s, 1);
    n1 += r.fiesta;
    nDerail += r.derailLane >= 0;
    raceBegin(r, s, 2);
    n2 += r.fiesta;
  }
  TEST_ASSERT_EQUAL(0, n0);
  TEST_ASSERT_EQUAL(4000 - nDerail, n1);
  TEST_ASSERT_INT_WITHIN(100, (4000 - nDerail) / 2, n2);
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
  RUN_TEST(test_fiesta_chance);
  RUN_TEST(test_positions_glide_and_settle);
  return UNITY_END();
}
