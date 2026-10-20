#pragma once
// Too long texts: wait, scroll to the end, wait, jump back. Used for the
// destinations (src/render.cpp) and the event lines (src/event_render.cpp).
#include <stdint.h>

static const uint32_t PING_PONG_PAUSE_MS = 2500;

// Pixel offset at `ms` for a text that is `overflow` px wider than its box,
// scrolling at `speedPxS`.
inline int pingPongOffset(int overflow, int speedPxS, uint32_t ms) {
  uint32_t scrollMs = (uint32_t)overflow * 1000 / speedPxS;
  uint32_t t = ms % (PING_PONG_PAUSE_MS + scrollMs + PING_PONG_PAUSE_MS);
  if (t < PING_PONG_PAUSE_MS) return 0;
  t -= PING_PONG_PAUSE_MS;
  return t < scrollMs ? (int)(t * speedPxS / 1000) : overflow;
}
