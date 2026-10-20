#include "race.h"

uint32_t raceRand(uint32_t& state) {
  if (!state) state = 0x9E3779B9;  // xorshift gets stuck on 0
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

void raceBegin(Race& r, uint32_t seed) {
  r.seed = seed;
  r.nMoves = 0;
  uint32_t rng = seed;
  for (int i = 0; i < 4; i++) raceRand(rng);  // close seeds give different races
  int pos[RACE_LANES] = {};
  uint32_t at = 0;
  for (;;) {
    at += 600 + raceRand(rng) % 351;
    int lane = raceRand(rng) % RACE_LANES;
    int roll = raceRand(rng) % 100;
    int delta = roll < 80 ? 1 : roll < 88 ? 2 : -1;
    // safety net: late in a long race only forward, so it fits RACE_MAX_MOVES
    if (delta < 0 && (pos[lane] == 0 || r.nMoves >= RACE_MAX_MOVES - RACE_LANES * RACE_FIELDS)) delta = 1;
    if (pos[lane] + delta > RACE_FIELDS) delta = RACE_FIELDS - pos[lane];
    pos[lane] += delta;
    r.moves[r.nMoves++] = {(uint16_t)(at / 10), (uint8_t)lane, (int8_t)delta};
    if (pos[lane] == RACE_FIELDS) {
      r.winner = lane;
      r.endMs = (at / 10) * 10 + RACE_SLIDE_MS;
      return;
    }
  }
}

uint32_t raceTotalMs(const Race& r) { return RACE_COUNTDOWN_MS + r.endMs + RACE_WIN_MS; }

void racePositions(const Race& r, uint32_t t, int32_t pos[RACE_LANES]) {
  int field[RACE_LANES] = {};
  for (int i = 0; i < RACE_LANES; i++) pos[i] = 0;
  for (int i = 0; i < r.nMoves; i++) {
    const RaceMove& m = r.moves[i];
    uint32_t start = m.at10ms * 10u;
    if (start > t) break;
    int from = field[m.lane];
    field[m.lane] += m.delta;
    uint32_t dt = t - start;
    if (dt >= RACE_SLIDE_MS) {
      pos[m.lane] = field[m.lane] * 256;
    } else {
      // ease in and out: 3x^2 - 2x^3
      int64_t x = (int64_t)dt * 256 / RACE_SLIDE_MS;
      int64_t e = (3 * x * x * 256 - 2 * x * x * x) / (256 * 256);
      pos[m.lane] = from * 256 + (int32_t)(m.delta * e);
    }
  }
}
