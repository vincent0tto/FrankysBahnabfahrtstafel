#include "race_render.h"

#include <stdio.h>

#include "config.h"
#include "depart_anim.h"

// ---- Layout (192 x 64) ------------------------------------------------------
// Four lanes of 16 px, each: start number, then the tram of the departure
// animation, its front at NOSE0 on field 0 (the rest of it still coming in
// from the left), FIELD_W px further per field. Its front passes the finish
// line on field 12.
static const int W = PANEL_RES_X * PANEL_CHAIN;
static const int H = PANEL_RES_Y;
static const int LANE_H = H / RACE_LANES;
static const int NUM_W = 10;        // start number column
static const int NOSE0 = 40;        // front of a tram on field 0
static const int FIELD_W = 12;
static const int START_X = NOSE0 + 2;                          // start line
static const int FINISH_X = NOSE0 + RACE_FIELDS * FIELD_W - 3;  // finish, 2 px
static const int PIVOT = 31;        // the tram's articulation column

#define FONT_NUM   u8g2_font_t0_13b_tf
#define FONT_COUNT u8g2_font_logisoso38_tf
#define FONT_WIN   u8g2_font_helvB14_tf

static uint16_t rgb565(uint32_t rgb) {
  return ((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x1F);
}

// Bright and clearly apart on the LEDs, none of them the derailment's red.
static const uint32_t LANE_RGB[RACE_LANES] = {0xFFD800, 0x00B4FF, 0x30E830, 0xFF30C8};
static const uint32_t DERAIL_RGB = 0xFF1010;
static const uint32_t GO_RGB = 0x30E830;
static const uint32_t LINE_RGB = 0xD8D8D8;  // lanes, fields, start, finish: white
static const uint32_t ICE_RGB = 0xFFFFFF, ICE_STRIPE_RGB = 0xFF1010;

// ICE power car and the end of the next car, as long as the tram: '#' white,
// 'r' its red stripe.
static const char* const ICE_ROWS[TRAM_H] = {
    ".....................................####......................",
    "......................................#........................",
    "#####################.#########################................",
    "#####################.############################.............",
    "#.....#.....#.....#.....#.....#.....#.....#..##....##..........",
    "#####################.###################################......",
    "#############################################################..",
    "rrrrrrrrrrrrrrrrrrrrr.rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr##",
    "#####################.######################################...",
    "....###.......###.........###.......###.........###............",
    ".....#.........#...........#.........#...........#.............",
};

static const uint32_t COUNT_STEP_MS = RACE_COUNTDOWN_MS / 4;
static const uint32_t BLINK_MS = 2000;  // winner blinks at the finish
static const uint32_t LAP_MS = RACE_WIN_MS - BLINK_MS;  // lap of honour
// derailment: drive to the middle, derail, then the text
static const uint32_t DRIVE_MS = 1600, CRASH_MS = 1200;
static const int RAIL_Y = 47;
static const int CRASH_TILT = 8;  // px the front car's nose ends up below the rail

// A train (`rows`: '#' `color`, 'r' `stripe`) with its front column at
// `nose`, its wheels on `bottom`; the front car tilted `tilt` px at its nose
// (down, negative: up). Nothing left of `clipL` or outside rows [clipT, clipB].
static void drawTrain(Adafruit_GFX& g, const char* const* rows, uint16_t color, uint16_t stripe, int nose,
                      int bottom, int tilt, int clipL, int clipT, int clipB) {
  int left = nose - (TRAM_W - 1);
  for (int r = 0; r < TRAM_H; r++)
    for (int c = 0; c < TRAM_W; c++) {
      if (rows[r][c] == '.') continue;
      int x = left + c;
      int y = bottom - (TRAM_H - 1) + r + (c > PIVOT ? (c - PIVOT) * tilt / (TRAM_W - 1 - PIVOT) : 0);
      if (x >= clipL && x < W && y >= clipT && y <= clipB) g.drawPixel(x, y, rows[r][c] == 'r' ? stripe : color);
    }
}

static void drawTram(Adafruit_GFX& g, uint16_t color, int nose, int bottom, int tilt, int clipL, int clipT,
                     int clipB) {
  drawTrain(g, TRAM_ROWS, color, color, nose, bottom, tilt, clipL, clipT, clipB);
}

static void drawIce(Adafruit_GFX& g, int nose, int bottom, int clipL, int clipT, int clipB) {
  drawTrain(g, ICE_ROWS, rgb565(ICE_RGB), rgb565(ICE_STRIPE_RGB), nose, bottom, 0, clipL, clipT, clipB);
}

// Colour and name of a lane's train: the ICE is "ICE 3".
static uint16_t laneColor(const Race& r, int lane) { return rgb565(lane == r.iceLane ? ICE_RGB : LANE_RGB[lane]); }
static const char* laneName(const Race& r, int lane) { return lane == r.iceLane ? "ICE" : "Bahn"; }

static void centeredText(U8G2_FOR_ADAFRUIT_GFX& u8, int x0, int x1, int base, const char* s) {
  u8.drawUTF8(x0 + (x1 - x0 - u8.getUTF8Width(s) + 1) / 2, base, s);
}

static int noseX(int32_t pos) { return NOSE0 + (int)(pos * FIELD_W / 256); }

// Lanes, fields, start and finish; the trains at `pos` (1/256 fields), lane
// `hide` left out (-1: none), lane `derailed` red and off its rails.
static void drawTrack(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Race& r, const int32_t pos[RACE_LANES],
                      int hide, int derailed) {
  uint16_t color = rgb565(LINE_RGB);
  u8.setFont(FONT_NUM);
  for (int i = 0; i < RACE_LANES; i++) {
    int top = i * LANE_H, line = top + LANE_H - 1;
    for (int x = NUM_W; x < W; x += 2) g.drawPixel(x, line, color);  // dotted lane border
    for (int k = 1; k < RACE_FIELDS; k++) g.drawFastVLine(START_X + k * FIELD_W, line - 1, 2, color);  // field marks
  }
  for (int y = 0; y < H; y += 2) g.drawPixel(START_X, y, color);
  for (int y = 0; y < H; y++)  // chequered 2 x 2
    g.drawPixel(FINISH_X + ((y / 2) & 1), y, color);
  for (int i = 0; i < RACE_LANES; i++) {
    int top = i * LANE_H, bottom = top + LANE_H - 3;
    uint16_t c = i == derailed ? rgb565(DERAIL_RGB) : laneColor(r, i);
    char num[4];
    snprintf(num, sizeof(num), "%d", i + 1);
    u8.setForegroundColor(c);
    centeredText(u8, 0, NUM_W, bottom, num);
    if (i == hide) continue;
    int nose = noseX(pos[i]);
    int left = nose - (TRAM_W - 1) > NUM_W + 1 ? nose - (TRAM_W - 1) : NUM_W + 1;
    g.fillRect(left, bottom - 8, nose - left + 1, 7, 0);  // no lines through the windows
    if (i == r.iceLane) drawIce(g, nose, bottom, NUM_W + 1, top, top + LANE_H - 1);
    else drawTram(g, c, nose, bottom, i == derailed ? -3 : 0, NUM_W + 1, top, top + LANE_H - 1);
  }
}

static int ease(int x, int span) {  // 0..256 for x in 0..span, 3x^2 - 2x^3
  int64_t p = (int64_t)x * 256 / span;
  return (int)((3 * p * p * 256 - 2 * p * p * p) / (256 * 256));
}

// The derailing tram alone on the board, `u` ms into RACE_DERAIL_MS.
static void drawDerail(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Race& r, uint32_t u) {
  int lane = r.derailLane;
  int32_t pos[RACE_LANES];
  racePositions(r, r.derailMs, pos);
  int nose0 = noseX(pos[lane]), bottom0 = lane * LANE_H + LANE_H - 3;
  int noseC = (W + TRAM_W) / 2, bottomC = RAIL_Y - 1;
  uint16_t red = rgb565(DERAIL_RGB);
  g.drawFastHLine(0, RAIL_Y, W, rgb565(LINE_RGB));

  if (u < DRIVE_MS) {
    int p = ease(u, DRIVE_MS);
    drawTram(g, red, nose0 + (noseC - nose0) * p / 256, bottom0 + (bottomC - bottom0) * p / 256, 0, 0, 0, H - 1);
    return;
  }
  if (u < DRIVE_MS + CRASH_MS) {
    uint32_t v = u - DRIVE_MS;
    int slide = (int)(v * CRASH_TILT / CRASH_MS);
    int shake = v < CRASH_MS * 6 / 10 ? ((v / 40) % 2 ? 1 : -1) : 0;
    drawTram(g, red, noseC + slide + shake, bottomC, (int)(v * CRASH_TILT / CRASH_MS), 0, 0, H - 1);
    // sparks at the wheels, fewer and fewer
    uint32_t rng = r.seed ^ (v / 50 + 7) * 0x9E3779B9u;
    int n = 16 - (int)(v * 16 / CRASH_MS);
    for (int i = 0; i < n; i++) {
      int x = noseC + slide - (int)(raceRand(rng) % TRAM_W);
      int y = RAIL_Y - 2 + (int)(raceRand(rng) % 5);
      g.drawPixel(x, y, rgb565(raceRand(rng) % 3 ? 0xFFD800 : 0xFFFFFF));
    }
    return;
  }
  drawTram(g, red, noseC + CRASH_TILT, bottomC, CRASH_TILT, 0, 0, H - 1);
  char text[24];
  snprintf(text, sizeof(text), "Bahn %d entgleist!", lane + 1);
  u8.setFont(FONT_WIN);
  u8.setForegroundColor(red);
  centeredText(u8, 0, W, 20, text);
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

// Fireworks, "Bahn N gewinnt!" and the winner driving through, `u` ms in.
static void drawWinner(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Race& r, uint32_t u) {
  uint16_t c = laneColor(r, r.winner);
  for (uint32_t b = 0; b * 700 <= u; b++) {
    uint32_t age = u - b * 700;
    if (age >= 1200) continue;
    uint32_t rng = r.seed ^ (b * 0x9E3779B9u + 1);
    raceRand(rng);
    int cx = 14 + raceRand(rng) % (W - 28);
    int cy = 10 + raceRand(rng) % 12;
    drawBurst(g, rgb565(LANE_RGB[b % RACE_LANES]), cx, cy, age);
  }
  char text[24];
  snprintf(text, sizeof(text), "%s %d gewinnt!", laneName(r, r.winner), r.winner + 1);
  u8.setFont(FONT_WIN);
  u8.setForegroundColor(c);
  int tw = u8.getUTF8Width(text), base = 44, asc = u8.getFontAscent();
  g.fillRect((W - tw) / 2 - 3, base - asc - 3, tw + 6, asc + 6, 0);
  centeredText(u8, 0, W, base, text);

  g.fillRect(0, H - 12, W, 12, 0);
  int nose = -1 + (int)((uint64_t)u * (W + TRAM_W) / LAP_MS);
  if (r.winner == r.iceLane) drawIce(g, nose, H - 1, 0, H - 12, H - 1);
  else drawTram(g, c, nose, H - 1, 0, 0, H - 12, H - 1);
}

// Places 1-4, one row each, coming in from the top, `u` ms in.
static void drawRanking(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Race& r, uint32_t u) {
  int order[RACE_LANES], field[RACE_LANES];
  raceRanking(r, order, field);
  u8.setFont(FONT_NUM);
  for (int i = 0; i < RACE_LANES && u >= (uint32_t)i * 300; i++) {
    int lane = order[i], base = i * LANE_H + LANE_H - 3;
    char s[16];
    snprintf(s, sizeof(s), "%d.", i + 1);
    u8.setForegroundColor(color);
    u8.drawUTF8(6, base, s);
    snprintf(s, sizeof(s), "%s %d", laneName(r, lane), lane + 1);
    u8.setForegroundColor(laneColor(r, lane));
    u8.drawUTF8(30, base, s);
    bool out = lane == r.derailLane || (lane == r.iceLane && r.iceCancelled);
    if (lane == r.derailLane) snprintf(s, sizeof(s), "entgleist");
    else if (out) snprintf(s, sizeof(s), "fällt aus");
    else if (field[lane] == RACE_FIELDS) snprintf(s, sizeof(s), "Sieger");
    else if (field[lane] == 0) snprintf(s, sizeof(s), "am Start");
    else snprintf(s, sizeof(s), "Feld %d", field[lane]);
    u8.setForegroundColor(out ? rgb565(DERAIL_RGB) : color);
    u8.drawUTF8(W - 6 - u8.getUTF8Width(s), base, s);
  }
}

void drawRace(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Race& r, uint32_t t) {
  u8.setFontMode(1);
  int32_t pos[RACE_LANES] = {};

  if (t < RACE_COUNTDOWN_MS) {
    drawTrack(g, u8, r, pos, -1, -1);
    static const char* const STEPS[] = {"3", "2", "1", "LOS!"};
    int step = t / COUNT_STEP_MS < 4 ? t / COUNT_STEP_MS : 3;
    u8.setFont(FONT_COUNT);
    u8.setForegroundColor(step == 3 ? rgb565(GO_RGB) : color);
    int tw = u8.getUTF8Width(STEPS[step]), asc = u8.getFontAscent();
    int bw = tw + 14, bh = asc + 12, bx = (W - bw) / 2, by = (H - bh) / 2;
    g.fillRect(bx, by, bw, bh, 0);
    g.drawRect(bx, by, bw, bh, rgb565(LINE_RGB));
    centeredText(u8, bx, bx + bw, by + 6 + asc, STEPS[step]);
    return;
  }
  t -= RACE_COUNTDOWN_MS;
  if (r.derailLane >= 0 && t >= r.derailMs && t < r.derailMs + RACE_DERAIL_MS) {
    drawDerail(g, u8, r, t - r.derailMs);
    return;
  }
  if (t < r.endMs + BLINK_MS) {
    racePositions(r, t, pos);
    bool blinkOff = t >= r.endMs && ((t - r.endMs) / 250) % 2;
    int derailed = r.derailLane >= 0 && t >= r.derailMs ? r.derailLane : -1;
    drawTrack(g, u8, r, pos, blinkOff ? r.winner : -1, derailed);
    return;
  }
  uint32_t u = t - r.endMs - BLINK_MS;
  if (u < LAP_MS) drawWinner(g, u8, r, u);
  else drawRanking(g, u8, color, r, u - LAP_MS);
}
