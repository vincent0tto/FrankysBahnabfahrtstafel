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
  r.derailLane = -1;
  r.derailMs = 0;
  r.iceLane = -1;
  r.iceCancelled = false;
  uint32_t rng = seed;
  for (int i = 0; i < 4; i++) raceRand(rng);  // close seeds give different races
  bool derail = raceRand(rng) % 10 == 0;
  int derailAt = 3 + raceRand(rng) % 6;  // leader's field
  bool ice = raceRand(rng) % 12 == 0;
  int iceLane = raceRand(rng) % RACE_LANES;
  bool iceCancelled = raceRand(rng) % 3 == 0;
  if (ice && !derail) {
    r.iceLane = iceLane;
    r.iceCancelled = iceCancelled;
  }
  int pos[RACE_LANES] = {};
  int lead = 0;
  uint32_t at = 0;
  for (;;) {
    at += 600 + raceRand(rng) % 351;
    if (derail && r.derailLane < 0 && lead >= derailAt) {
      r.derailLane = raceRand(rng) % RACE_LANES;
      r.derailMs = at;
      at += RACE_DERAIL_MS;
      continue;
    }
    if (r.iceLane >= 0 && !r.iceCancelled && lead >= RACE_FIELDS / 2) {
      r.moves[r.nMoves++] = {(uint16_t)(at / 10), (uint8_t)r.iceLane, (int8_t)RACE_FIELDS};
      r.winner = r.iceLane;
      r.endMs = (at / 10) * 10 + RACE_ICE_MS;
      return;
    }
    // a random lane of those still running (the ICE waits)
    int out = r.derailLane >= 0 ? r.derailLane : r.iceLane;
    int lane = raceRand(rng) % (out < 0 ? RACE_LANES : RACE_LANES - 1);
    if (out >= 0 && lane >= out) lane++;
    int roll = raceRand(rng) % 100;
    int delta = roll < 80 ? 1 : roll < 88 ? 2 : -1;
    // safety net: late in a long race only forward, so it fits RACE_MAX_MOVES
    if (delta < 0 && (pos[lane] == 0 || r.nMoves >= RACE_MAX_MOVES - RACE_LANES * RACE_FIELDS)) delta = 1;
    if (pos[lane] + delta > RACE_FIELDS) delta = RACE_FIELDS - pos[lane];
    pos[lane] += delta;
    if (pos[lane] > lead) lead = pos[lane];
    r.moves[r.nMoves++] = {(uint16_t)(at / 10), (uint8_t)lane, (int8_t)delta};
    if (pos[lane] == RACE_FIELDS) {
      r.winner = lane;
      r.endMs = (at / 10) * 10 + RACE_SLIDE_MS;
      return;
    }
  }
}

uint32_t raceTotalMs(const Race& r) { return RACE_COUNTDOWN_MS + r.endMs + RACE_WIN_MS + RACE_RANK_MS; }

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
    bool ice = m.delta == RACE_FIELDS;
    uint32_t dur = ice ? RACE_ICE_MS : RACE_SLIDE_MS;
    if (dt >= dur) {
      pos[m.lane] = field[m.lane] * 256;
    } else {
      // a tram eases in and out (3x^2 - 2x^3), the ICE speeds up all the way (x^2)
      int64_t x = (int64_t)dt * 256 / dur;
      int64_t e = ice ? x * x / 256 : (3 * x * x * 256 - 2 * x * x * x) / (256 * 256);
      pos[m.lane] = from * 256 + (int32_t)(m.delta * e);
    }
  }
}

void raceRanking(const Race& r, int order[RACE_LANES], int field[RACE_LANES]) {
  int since[RACE_LANES] = {};  // move index from which the lane is on its field
  for (int i = 0; i < RACE_LANES; i++) field[i] = 0, order[i] = i;
  for (int i = 0; i < r.nMoves; i++) {
    field[r.moves[i].lane] += r.moves[i].delta;
    since[r.moves[i].lane] = i;
  }
  int out = r.derailLane >= 0 ? r.derailLane : r.iceCancelled ? r.iceLane : -1;
  auto ahead = [&](int a, int b) {  // a ranks before b
    if ((a == out) != (b == out)) return b == out;
    if (field[a] != field[b]) return field[a] > field[b];
    return since[a] < since[b];
  };
  for (int i = 1; i < RACE_LANES; i++)
    for (int j = i; j > 0 && ahead(order[j], order[j - 1]); j--) {
      int t = order[j];
      order[j] = order[j - 1];
      order[j - 1] = t;
    }
}
