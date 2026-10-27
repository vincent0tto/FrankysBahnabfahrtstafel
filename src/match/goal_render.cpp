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

// fireworks in St. Pauli's colours from `from` ms on, centred in x0..x1
static void drawBursts(Adafruit_GFX& g, uint32_t t, uint32_t from, int x0, int x1) {
  for (uint32_t b = 0; b * BURST_EVERY_MS + from <= t; b++) {
    uint32_t age = t - from - b * BURST_EVERY_MS;
    if (age >= 1200) continue;
    uint32_t rng = (b + 1) * 0x9E3779B9u;
    raceRand(rng);
    int cx = x0 + 14 + raceRand(rng) % (x1 - x0 - 28);
    int cy = 8 + raceRand(rng) % (H - 16);
    drawFireworkBurst(g, rgb565(GOAL_RGB[b % 3]), cx, cy, age);
  }
}

// The match's line in FONT_SCORE, centred on `cx` at the bottom, with the
// team codes if the names are wider than `maxW`; `fallback` without a match.
static void drawScore(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, const char* fallback, int cx,
                      int maxW) {
  char score[64];
  u8.setFont(FONT_SCORE);
  if (m.id) {
    matchLineText(m, false, score, sizeof(score));
    if (u8.getUTF8Width(score) > maxW) matchLineText(m, true, score, sizeof(score));
  } else {
    snprintf(score, sizeof(score), "%s", fallback);
  }
  int sw = u8.getUTF8Width(score), sbase = H - 4, sasc = u8.getFontAscent();
  g.fillRect(cx - sw / 2 - 3, sbase - sasc - 2, sw + 6, sasc + 5, 0);
  u8.setForegroundColor(rgb565(0xFFFFFF));
  u8.drawUTF8(cx - sw / 2, sbase, score);
}

void drawGoal(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, uint32_t t) {
  u8.setFontMode(1);

  // fireworks behind the text, from the second "O" on
  drawBursts(g, t, GROW_MS, 0, W);

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

  if (t >= SCORE_MS) drawScore(g, u8, m, "FC St. Pauli", W / 2, W - 8);
}

// ---- The HSV concedes -------------------------------------------------------

#define FONT_KASSIERT u8g2_font_logisoso20_tf
#define FONT_RAUTE    u8g2_font_helvB10_tf

// The logo: a blue field, in it a white Raute with a black one and a small
// white one inside. The blue is lighter than the real one (0x1A26AF).
static const uint32_t HSV_BLUE = 0x1E46F0;
static const int LOGO_W = 58, LOGO_H = 40;
static const int LOGO_CX = 2 + LOGO_W / 2, LOGO_CY = H / 2;
static const int TEXT_X0 = LOGO_W + 6;  // the text right of the logo
static const uint32_t FLY_MS = 700;     // the ball's flight into the Raute
static const uint32_t SHAKE_MS = 900;   // then the logo shakes this long

// a Raute (a square on its corner) with half-diagonal `r`
static void fillRaute(Adafruit_GFX& g, int cx, int cy, int r, uint16_t c) {
  for (int dy = -r; dy <= r; dy++) {
    int w = r - (dy < 0 ? -dy : dy);
    g.drawFastHLine(cx - w, cy + dy, 2 * w + 1, c);
  }
}

static void drawHsvLogo(Adafruit_GFX& g, int cx, int cy) {
  g.fillRect(cx - LOGO_W / 2, cy - LOGO_H / 2, LOGO_W, LOGO_H, rgb565(HSV_BLUE));
  int r = LOGO_H / 2 - 2;
  fillRaute(g, cx, cy, r, rgb565(0xFFFFFF));
  fillRaute(g, cx, cy, r * 3 / 4, 0);
  fillRaute(g, cx, cy, r / 3, rgb565(0xFFFFFF));
}

static void drawBall(Adafruit_GFX& g, int cx, int cy) {
  g.fillCircle(cx, cy, 4, rgb565(0xFFFFFF));
  g.fillRect(cx - 1, cy - 1, 3, 3, 0);
  g.drawPixel(cx - 3, cy + 1, 0);
  g.drawPixel(cx + 3, cy + 1, 0);
  g.drawPixel(cx, cy - 4, 0);
}

void drawConceded(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, uint32_t t) {
  u8.setFontMode(1);
  drawBursts(g, t, FLY_MS, TEXT_X0, W);

  // the ball comes in from the right in an arc and goes into the Raute,
  // which then shakes ever less
  int shake = 0;
  if (t >= FLY_MS && t < FLY_MS + SHAKE_MS) {
    int amp = 4 - (int)((t - FLY_MS) * 4 / SHAKE_MS);
    shake = ((t / 60) % 2 ? amp : -amp);
  }
  drawHsvLogo(g, LOGO_CX + shake, LOGO_CY);
  if (t < FLY_MS) {
    int x0 = W + 5, x1 = LOGO_CX;
    int x = x0 + (int)((int32_t)(x1 - x0) * (int32_t)t / (int32_t)FLY_MS);
    int f = (int)(t * 100 / FLY_MS);  // 0..100
    int y = LOGO_CY - (H / 2 - 8) * f * (100 - f) / 2500 + 2;
    drawBall(g, x, y);
    return;
  }

  int textCx = (TEXT_X0 + W) / 2;
  bool score = t >= SCORE_MS;
  u8.setFont(FONT_RAUTE);
  int rAsc = u8.getFontAscent();
  u8.setFont(FONT_KASSIERT);
  int kAsc = u8.getFontAscent();
  // both lines in the middle, moved up for the score
  int top = score ? 3 : (H - rAsc - 4 - kAsc) / 2;
  int rBase = top + rAsc, kBase = rBase + 4 + kAsc;

  u8.setFont(FONT_RAUTE);
  int rw = u8.getUTF8Width("DIE RAUTE");
  g.fillRect(textCx - rw / 2 - 2, rBase - rAsc - 2, rw + 4, rAsc + 4, 0);
  u8.setForegroundColor(rgb565(0xFFFFFF));
  u8.drawUTF8(textCx - rw / 2, rBase, "DIE RAUTE");

  u8.setFont(FONT_KASSIERT);
  int kw = u8.getUTF8Width("KASSIERT!");
  g.fillRect(textCx - kw / 2 - 3, kBase - kAsc - 3, kw + 6, kAsc + 6, 0);
  u8.setForegroundColor(rgb565(GOAL_RGB[((t - FLY_MS) / BLINK_MS) % 3]));
  u8.drawUTF8(textCx - kw / 2, kBase, "KASSIERT!");

  if (score) drawScore(g, u8, m, "Hamburger SV", textCx, W - TEXT_X0 - 4);
}
