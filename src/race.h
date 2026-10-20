#pragma once
// Tram race, like the horse race at the fair: four trams (numbers 1-4, one
// per row) move up field by field at random, now and then one back (rnv
// trams drive both ways), until one reaches the last field. About one race
// in ten a tram derails and stays where it is; one in 18 an ICE takes a
// lane, waits and then runs straight to the finish (or, one time in eight,
// is cancelled and never leaves). The whole race is worked out
// from a seed at the start, so drawing it (src/race_render.h) needs no
// state. No Arduino dependency, unit-tested on the host.
#include <stdint.h>

#define RACE_LANES   4
#define RACE_FIELDS 12   // the finish; the trams start on field 0 behind the start line
#define RACE_MAX_MOVES 128

static const uint32_t RACE_COUNTDOWN_MS = 3200;  // "3", "2", "1", "LOS!"
static const uint32_t RACE_SLIDE_MS = 300;       // a tram gliding to its new field
static const uint32_t RACE_ICE_MS = 1800;        // the ICE's run to the finish
static const uint32_t RACE_DERAIL_MS = 6000;     // the race pauses for the derailment
static const uint32_t RACE_WIN_MS = 8500;        // winner blinks, then the lap of honour
static const uint32_t RACE_RANK_MS = 5000;       // the ranking

struct RaceMove {
  uint16_t at10ms;  // when it starts, in 10 ms after the countdown
  uint8_t lane;
  int8_t delta;     // +1, +2 or -1; the ICE: RACE_FIELDS
};

struct Race {
  uint32_t seed = 0;
  RaceMove moves[RACE_MAX_MOVES];
  int nMoves = 0;
  int winner = 0;        // lane 0..3
  uint32_t endMs = 0;    // after the countdown: the winner has arrived
  int derailLane = -1;   // -1: none
  uint32_t derailMs = 0; // after the countdown: the derailment starts (RACE_DERAIL_MS long)
  int iceLane = -1;      // the ICE's lane, -1: none
  bool iceCancelled = false;  // it never leaves and ranks last
};

// Works out the race for `seed`. Every 600-950 ms exactly one tram, picked
// at random, moves: +1 (80 %), +2 (8 %) or -1 (12 %, +1 on the start
// field), so each has the same chance and there is always one winner. A
// race takes ~30 s on average. In 1 of 10 races, once the leader is on
// field 3-8, a random tram derails; it moves no more and ranks last.
// Otherwise in 1 of 18 an ICE takes a random lane: it stands until the
// leader is half-way, then runs to the finish in one go and wins; in 1 of 8
// of those it is cancelled instead, stands all race and ranks last.
void raceBegin(Race& r, uint32_t seed);

// Countdown + race + winner animation + ranking.
uint32_t raceTotalMs(const Race& r);

// Field of each lane `t` ms after the countdown, in 1/256 fields (gliding).
void racePositions(const Race& r, uint32_t t, int32_t pos[RACE_LANES]);

// Lanes from first to last at the end, and the field each ended on: further
// is better, on the same field the one that got there first, a derailed
// tram or a cancelled ICE last.
void raceRanking(const Race& r, int order[RACE_LANES], int field[RACE_LANES]);

// Small deterministic random numbers (xorshift32), also for the sparkles.
uint32_t raceRand(uint32_t& state);
