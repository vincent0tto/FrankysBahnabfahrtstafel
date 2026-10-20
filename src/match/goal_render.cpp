#include "match/goal_render.h"

#include <stdio.h>

#include "config.h"
#include "event/event.h"
#include "race/race.h"
#include "race/race_render.h"

static const int W = PANEL_RES_X * PANEL_CHAIN;
static const int H = PANEL_RES_Y;

#define FONT_GOAL   u8g2_font_logisoso32_tf
#define FONT_GOAL_S u8g2_font_logisoso24_tf  // if "TOOOOR!" is too wide
#define FONT_SCORE  u8g2_font_helvB10_tf

// St. Pauli: brown, white, red. The brown is lighter than the real one,
// a dark one hardly shows on the LEDs.
static const uint32_t GOAL_RGB[3] = {0xC06018, 0xFFFFFF, 0xFF1010};

static const uint32_t GROW_MS = 250;     // one more "O" every this long
static const int MAX_O = 4;
static const uint32_t BLINK_MS = 300;    // next colour
static const uint32_t SCORE_MS = 2000;   // the score comes up
static const uint32_t BURST_EVERY_MS = 450;

void drawGoal(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, uint32_t t) {
  u8.setFontMode(1);

  // fireworks behind the text, from the second "O" on
  for (uint32_t b = 0; b * BURST_EVERY_MS + GROW_MS <= t; b++) {
    uint32_t age = t - GROW_MS - b * BURST_EVERY_MS;
    if (age >= 1200) continue;
    uint32_t rng = (b + 1) * 0x9E3779B9u;
    raceRand(rng);
    int cx = 14 + raceRand(rng) % (W - 28);
    int cy = 8 + raceRand(rng) % (H - 16);
    drawFireworkBurst(g, rgb565(GOAL_RGB[b % 3]), cx, cy, age);
  }

  char text[16] = "T";
  int os = 1 + (int)(t / GROW_MS);
  if (os > MAX_O) os = MAX_O;
  int len = 1;
  for (int i = 0; i < os; i++) text[len++] = 'O';
  snprintf(text + len, sizeof(text) - len, "R!");
  u8.setFont(FONT_GOAL);
  int tw = u8.getUTF8Width(text);
  if (tw > W - 8) {
    u8.setFont(FONT_GOAL_S);
    tw = u8.getUTF8Width(text);
  }
  int asc = u8.getFontAscent(), base = t < SCORE_MS ? (H + asc) / 2 : asc + 6;
  g.fillRect((W - tw) / 2 - 3, base - asc - 3, tw + 6, asc + 6, 0);
  u8.setForegroundColor(rgb565(GOAL_RGB[(t / BLINK_MS) % 3]));
  u8.drawUTF8((W - tw) / 2, base, text);

  if (t < SCORE_MS) return;
  char score[64];
  u8.setFont(FONT_SCORE);
  if (m.id) {
    matchLineText(m, false, score, sizeof(score));
    if (u8.getUTF8Width(score) > W - 8) matchLineText(m, true, score, sizeof(score));
  } else {
    snprintf(score, sizeof(score), "FC St. Pauli");
  }
  int sw = u8.getUTF8Width(score), sbase = H - 4, sasc = u8.getFontAscent();
  g.fillRect((W - sw) / 2 - 3, sbase - sasc - 2, sw + 6, sasc + 5, 0);
  u8.setForegroundColor(rgb565(0xFFFFFF));
  u8.drawUTF8((W - sw) / 2, sbase, score);
}
