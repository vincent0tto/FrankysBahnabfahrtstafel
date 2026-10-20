#include "race_render.h"

#include <stdio.h>

#include "config.h"
#include "depart_anim.h"

// ---- Layout (192 x 64) ------------------------------------------------------
// Four lanes of 16 px, each: start number, start box (field 0), fields 1-12
// of FIELD_W px. The finish line sits right before field 12.
static const int W = PANEL_RES_X * PANEL_CHAIN;
static const int H = PANEL_RES_Y;
static const int LANE_H = H / RACE_LANES;
static const int NUM_W = 10;   // start number column
static const int FIELD_W = (W - NUM_W) / (RACE_FIELDS + 1);  // 14
static const int START_X = NUM_W + FIELD_W - 1;              // start line
static const int FINISH_X = NUM_W + RACE_FIELDS * FIELD_W - 1;  // finish, 2 px

#define FONT_NUM   u8g2_font_t0_13b_tf
#define FONT_COUNT u8g2_font_logisoso38_tf
#define FONT_WIN   u8g2_font_helvB14_tf

// Small one-car tram facing right, pantograph on top; stands on its last row.
static const int CAR_W = 12, CAR_H = 9;
static const char* const CAR[CAR_H] = {
    "....#####...",
    ".....#.#....",
    "###########.",
    "#..#..#..#.#",
    "#..#..#..#..",
    "############",
    "############",
    "############",
    ".##.....##..",
};

static const uint32_t COUNT_STEP_MS = RACE_COUNTDOWN_MS / 4;
static const uint32_t BLINK_MS = 2000;  // winner blinks at the finish
static const uint32_t LAP_MS = RACE_WIN_MS - BLINK_MS;  // lap of honour

static void drawCar(Adafruit_GFX& g, uint16_t color, int x, int bottom) {
  for (int r = 0; r < CAR_H; r++)
    for (int c = 0; c < CAR_W; c++)
      if (CAR[r][c] == '#') g.drawPixel(x + c, bottom - (CAR_H - 1) + r, color);
}

static void centeredText(U8G2_FOR_ADAFRUIT_GFX& u8, int x0, int x1, int base, const char* s) {
  u8.drawUTF8(x0 + (x1 - x0 - u8.getUTF8Width(s) + 1) / 2, base, s);
}

// Lanes, fields, start and finish; the trams at `pos` (1/256 fields), lane
// `hide` left out (-1: none).
static void drawTrack(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const int32_t pos[RACE_LANES],
                      int hide) {
  u8.setFont(FONT_NUM);
  for (int i = 0; i < RACE_LANES; i++) {
    int top = i * LANE_H, line = top + LANE_H - 1;
    char num[4];
    snprintf(num, sizeof(num), "%d", i + 1);
    centeredText(u8, 0, NUM_W, line - 2, num);
    for (int x = NUM_W; x < W; x += 2) g.drawPixel(x, line, color);  // dotted lane border
    for (int k = 2; k < RACE_FIELDS; k++) g.drawFastVLine(NUM_W + k * FIELD_W, line - 1, 2, color);  // field marks
    if (i != hide) drawCar(g, color, NUM_W + 1 + (int)(pos[i] * FIELD_W / 256), line - 2);
  }
  for (int y = 0; y < H; y += 2) g.drawPixel(START_X, y, color);
  for (int y = 0; y < H; y++)  // chequered 2 x 2
    g.drawPixel(FINISH_X + ((y / 2) & 1), y, color);
}

// A firework burst `age` ms old (of 1200) around (cx, cy).
static void drawBurst(Adafruit_GFX& g, uint16_t color, int cx, int cy, uint32_t age) {
  static const int8_t DIR[12][2] = {{16, 0}, {14, 8}, {8, 14}, {0, 16}, {-8, 14}, {-14, 8},
                                    {-16, 0}, {-14, -8}, {-8, -14}, {0, -16}, {8, -14}, {14, -8}};
  int r = 2 + (int)(age * 12 / 1200);
  for (int d = 0; d < 12; d++) {
    if (age > 900 && (d + age / 100) % 2) continue;  // fading out
    g.drawPixel(cx + DIR[d][0] * r / 16, cy + DIR[d][1] * r / 16, color);
    if (age < 600) g.drawPixel(cx + DIR[d][0] * (r - 2) / 16, cy + DIR[d][1] * (r - 2) / 16, color);
  }
}

// Fireworks, "Bahn N gewinnt!" and the big tram driving through, `u` ms in.
static void drawWinner(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Race& r, uint32_t u) {
  for (uint32_t b = 0; b * 700 <= u; b++) {
    uint32_t age = u - b * 700;
    if (age >= 1200) continue;
    uint32_t rng = r.seed ^ (b * 0x9E3779B9u + 1);
    raceRand(rng);
    int cx = 14 + raceRand(rng) % (W - 28);
    int cy = 10 + raceRand(rng) % 12;
    drawBurst(g, color, cx, cy, age);
  }
  char text[24];
  snprintf(text, sizeof(text), "Bahn %d gewinnt!", r.winner + 1);
  u8.setFont(FONT_WIN);
  int tw = u8.getUTF8Width(text), base = 44, asc = u8.getFontAscent();
  g.fillRect((W - tw) / 2 - 3, base - asc - 3, tw + 6, asc + 6, 0);
  centeredText(u8, 0, W, base, text);

  g.fillRect(0, H - 12, W, 12, 0);
  int x = -63 + (int)((uint64_t)u * (W + 63) / LAP_MS);
  drawVehicle(g, color, true, x, H - 1, H - 12);
}

void drawRace(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Race& r, uint32_t t) {
  u8.setFontMode(1);
  u8.setForegroundColor(color);
  int32_t pos[RACE_LANES] = {};

  if (t < RACE_COUNTDOWN_MS) {
    drawTrack(g, u8, color, pos, -1);
    static const char* const STEPS[] = {"3", "2", "1", "LOS!"};
    const char* s = STEPS[t / COUNT_STEP_MS < 4 ? t / COUNT_STEP_MS : 3];
    u8.setFont(FONT_COUNT);
    int tw = u8.getUTF8Width(s), asc = u8.getFontAscent();
    int bw = tw + 14, bh = asc + 12, bx = (W - bw) / 2, by = (H - bh) / 2;
    g.fillRect(bx, by, bw, bh, 0);
    g.drawRect(bx, by, bw, bh, color);
    centeredText(u8, bx, bx + bw, by + 6 + asc, s);
    return;
  }
  t -= RACE_COUNTDOWN_MS;
  if (t < r.endMs + BLINK_MS) {
    racePositions(r, t, pos);
    bool blinkOff = t >= r.endMs && ((t - r.endMs) / 250) % 2;
    drawTrack(g, u8, color, pos, blinkOff ? r.winner : -1);
    return;
  }
  uint32_t u = t - r.endMs - BLINK_MS;
  drawWinner(g, u8, color, r, u < LAP_MS ? u : LAP_MS);
}
