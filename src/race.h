#pragma once
// Tram race, like the horse race at the fair: four trams (numbers 1-4, one
// per row) move up field by field at random, now and then one back (rnv
// trams drive both ways), until one reaches the last field. The whole race
// is worked out from a seed at the start, so drawing it (src/race_render.h)
// needs no state. No Arduino dependency, unit-tested on the host.
#include <stdint.h>

#define RACE_LANES   4
#define RACE_FIELDS 12   // the finish; the trams start on field 0 before the start line
#define RACE_MAX_MOVES 128

static const uint32_t RACE_COUNTDOWN_MS = 3200;  // "3", "2", "1", "LOS!"
static const uint32_t RACE_SLIDE_MS = 300;       // a tram gliding to its new field
static const uint32_t RACE_WIN_MS = 8500;        // winner blinks, then the lap of honour

struct RaceMove {
  uint16_t at10ms;  // when it starts, in 10 ms after the countdown
  uint8_t lane;
  int8_t delta;     // +1, +2 or -1
};

struct Race {
  uint32_t seed = 0;
  RaceMove moves[RACE_MAX_MOVES];
  int nMoves = 0;
  int winner = 0;       // lane 0..3
  uint32_t endMs = 0;   // after the countdown: the winner has arrived
};

// Works out the race for `seed`. Every 600-950 ms exactly one tram, picked
// at random, moves: +1 (80 %), +2 (8 %) or -1 (12 %, +1 on the start
// field), so each has the same chance and there is always one winner. A
// race takes ~30 s on average.
void raceBegin(Race& r, uint32_t seed);

// Countdown + race + winner animation.
uint32_t raceTotalMs(const Race& r);

// Field of each lane `t` ms after the countdown, in 1/256 fields (gliding).
void racePositions(const Race& r, uint32_t t, int32_t pos[RACE_LANES]);

// Small deterministic random numbers (xorshift32), also for the sparkles.
uint32_t raceRand(uint32_t& state);
