#include "match/goal_render.h"

#include <math.h>
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
static const int LOGO_X0 = LOGO_CX - LOGO_W / 2, LOGO_Y0 = LOGO_CY - LOGO_H / 2;
static const int TEXT_X0 = LOGO_W + 6;  // the text right of the logo
static const uint32_t FLY_MS = 700;     // the ball's flight into the Raute
static const float GRAV = 0.00012f;     // px/ms², falling debris

// after the ball's flight: the logo breaks, then comes the text, per animation
enum { BREAK_BOOM, BREAK_TEAR, BREAK_SHATTER, BREAK_MELT, BREAK_CANNON, BREAK_DRAIN };
static const uint32_t TEXT_MS[CONCEDED_ANIMS] = {500, 1200, 800, 1100, 1900, 1400};
static const uint32_t SCORE_AFTER_MS = 1300;  // the score after the text

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

// the logo's pixel at x 0..LOGO_W-1, y 0..LOGO_H-1, as drawHsvLogo() draws it
static uint32_t logoRgb(int x, int y) {
  int dx = x - LOGO_W / 2, dy = y - LOGO_H / 2;
  int d = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy), r = LOGO_H / 2 - 2;
  if (d <= r / 3) return 0xFFFFFF;
  if (d <= r * 3 / 4) return 0;
  if (d <= r) return 0xFFFFFF;
  return HSV_BLUE;
}

// `a` towards `b` by f (0..256)
static uint32_t mixRgb(uint32_t a, uint32_t b, int f) {
  uint32_t out = 0;
  for (int sh = 0; sh <= 16; sh += 8) {
    int ca = (a >> sh) & 0xFF, cb = (b >> sh) & 0xFF;
    out |= (uint32_t)(ca + (cb - ca) * f / 256) << sh;
  }
  return out;
}

// a random 0..n-1 from `seed` and two numbers, the same every frame
static int hashRand(uint32_t seed, uint32_t a, uint32_t b, int n) {
  uint32_t rng = seed ^ (a * 0x9E3779B9u) ^ (b * 0x85EBCA6Bu);
  raceRand(rng);
  return (int)(raceRand(rng) % (uint32_t)n);
}

static float randF(uint32_t& rng, float lo, float hi) {
  return lo + (hi - lo) * (float)(raceRand(rng) % 1000) / 1000.0f;
}

static void drawBall(Adafruit_GFX& g, int cx, int cy) {
  g.fillCircle(cx, cy, 4, rgb565(0xFFFFFF));
  g.fillRect(cx - 1, cy - 1, 3, 3, 0);
  g.drawPixel(cx - 3, cy + 1, 0);
  g.drawPixel(cx + 3, cy + 1, 0);
  g.drawPixel(cx, cy - 4, 0);
}

// the ball from the right in an arc to (tx, ty), `f` 0..100 of the flight
static void drawBallFlight(Adafruit_GFX& g, int tx, int ty, int f) {
  int x0 = W + 5;
  int x = x0 + (tx - x0) * f / 100;
  int y = ty - (H / 2 - 8) * f * (100 - f) / 2500 + 2;
  drawBall(g, x, y);
}

// Explosion: a flash over the whole board, a fireball, the logo flying apart
// in glowing blocks, everything shaking.
static void breakBoom(Adafruit_GFX& g, uint32_t tau, uint32_t seed) {
  if (tau < 220) {
    int v = 255 - (int)tau * 255 / 220;
    g.fillScreen(rgb565((uint32_t)v << 16 | (uint32_t)v << 8 | (uint32_t)(v / 3)));
  }
  int sx = 0, sy = 0;
  if (tau < 700) {
    int amp = 5 - (int)tau * 5 / 700;
    sx = hashRand(seed, tau / 40, 1, 2 * amp + 1) - amp;
    sy = hashRand(seed, tau / 40, 2, 2 * amp + 1) - amp;
  }
  float ft = (float)tau;

  // the fireball: puffs flying out, white, yellow, orange, red, then smoke
  static const uint32_t FIRE[5] = {0xFFFFFF, 0xFFE040, 0xFF7000, 0xC01000, 0x302828};
  for (int i = 0; i < 16; i++) {
    uint32_t rng = seed ^ (i + 1) * 0x27D4EB2Fu;
    raceRand(rng);
    float ang = randF(rng, 0, 6.2832f), sp = randF(rng, 0.015f, 0.06f), life = randF(rng, 700, 1300);
    float age = ft - i * 12;
    if (age < 0 || age >= life) continue;
    float dist = sp * age * (1 - age / (2 * life));
    int px = LOGO_CX + sx + (int)(cosf(ang) * dist * 1.6f);
    int py = LOGO_CY + sy + (int)(sinf(ang) * dist) - (int)(0.012f * age);
    int r = 3 + (int)(11 * sinf(3.1416f * age / life));
    g.fillCircle(px, py, r, rgb565(FIRE[(int)(age * 5 / life)]));
  }

  // the logo in 3×3 blocks, flying away from the ball, some glowing
  for (int by = 0; by < LOGO_H; by += 3)
    for (int bx = 0; bx < LOGO_W; bx += 3) {
      uint32_t rng = seed ^ ((uint32_t)(by * 64 + bx + 1) * 0x9E3779B9u);
      raceRand(rng);
      float dx = bx + 1.5f - LOGO_W / 2, dy = by + 1.5f - LOGO_H / 2;
      float d = sqrtf(dx * dx + dy * dy) + 2;
      float sp = randF(rng, 0.05f, 0.25f);
      float vx = dx / d * sp * 1.4f + randF(rng, -0.03f, 0.03f);
      float vy = dy / d * sp + randF(rng, -0.12f, -0.02f);
      int ox = LOGO_X0 + bx + sx + (int)(vx * ft), oy = LOGO_Y0 + by + sy + (int)(vy * ft + 0.5f * GRAV * ft * ft);
      if (oy > H || ox < -3 || ox > W) continue;
      bool ember = raceRand(rng) % 3 == 0;
      int heat = tau < 900 ? 256 - (int)tau * 256 / 900 : 0;
      for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) {
          if (bx + i >= LOGO_W || by + j >= LOGO_H) continue;
          uint32_t c = logoRgb(bx + i, by + j);
          if (ember) c = (tau / 70 + bx) % 2 ? 0xFF5000 : 0xFFB000;
          else if (c) c = mixRgb(c, 0xFF8020, heat / 2);
          if (c) g.drawPixel(ox + i, oy + j, rgb565(c));
        }
    }
}

// Zerreißen: torn from top to bottom along a ragged line, the halves fly
// apart, twist and fall, with shreds and frayed white edges.
static void breakTear(Adafruit_GFX& g, uint32_t tau, uint32_t seed) {
  const uint32_t RIP_MS = 600, HANG_MS = 250;  // tearing, then the halves hang apart a moment
  float rip = tau < RIP_MS ? (float)tau / RIP_MS : 1;
  float ripRow = rip * (LOGO_H + 8);  // the tear has come down this far
  float gap = 1 + rip * 4;
  float a = tau > RIP_MS ? (float)(tau - RIP_MS) : 0;
  float twist = a * 0.0012f;  // the halves turn away (as a shear)
  float fa = a > HANG_MS ? a - HANG_MS : 0;
  float fall = 0.5f * GRAV * fa * fa;
  int jig = tau < RIP_MS ? hashRand(seed, tau / 50, 3, 3) - 1 : 0;

  uint32_t rng = seed;
  raceRand(rng);
  int tear = LOGO_W / 2;
  for (int y = 0; y < LOGO_H; y++) {
    tear += (int)(raceRand(rng) % 3) - 1;
    if (tear < LOGO_W / 2 - 5) tear = LOGO_W / 2 - 5;
    if (tear > LOGO_W / 2 + 5) tear = LOGO_W / 2 + 5;
    float open = ripRow - y;
    open = open < 0 ? 0 : (open > 8 ? 1 : open / 8);
    for (int side = 0; side < 2; side++) {
      // the left half falls to the left, its top first, the right one the same way round
      float lever = (float)(LOGO_H - y);
      float dx = (side ? 1 : -1) * (gap * open + a * 0.03f + twist * lever);
      int oy = LOGO_Y0 + y + (int)(fall + (side ? 0.6f : 1.0f) * twist * lever * 0.3f) + (side ? -jig : jig);
      int x0 = side ? tear : 0, x1 = side ? LOGO_W : tear;
      for (int x = x0; x < x1; x++) {
        uint32_t c = logoRgb(x, y);
        // frayed paper at the tear
        if (open > 0 && (x == tear - 1 || x == tear) && (raceRand(rng) & 1)) c = 0xFFFFFF;
        if (c) g.drawPixel(LOGO_X0 + x + (int)dx, oy, rgb565(c));
      }
    }
  }

  // shreds out of the tear
  if (tau > 150) {
    float st = (float)(tau - 150);
    for (int i = 0; i < 28; i++) {
      uint32_t r2 = seed ^ (i + 7) * 0x165667B1u;
      raceRand(r2);
      float y0 = randF(r2, 0, LOGO_H), born = y0 / LOGO_H * RIP_MS;
      if (st < born) continue;
      float s = st - born;
      float vx = randF(r2, -0.09f, 0.09f), vy = randF(r2, -0.1f, 0.0f);
      int x = LOGO_CX + (int)(vx * s), y = LOGO_Y0 + (int)(y0 + vy * s + 0.5f * GRAV * 0.6f * s * s);
      uint16_t c = rgb565(i % 3 ? 0xFFFFFF : HSV_BLUE);
      g.drawPixel(x, y, c);
      if (i % 2) g.drawPixel(x + 1, y, c);
    }
  }
}

// Zerspringen: cracks run out from the ball, then the logo shatters into
// shards that pop out, fall and lie on the floor glinting.
static void breakShatter(Adafruit_GFX& g, uint32_t tau, uint32_t seed) {
  const int N = 18;
  const uint32_t CRACK_MS = 450;
  float sxs[N], sys[N];
  uint32_t rng = seed;
  raceRand(rng);
  sxs[0] = LOGO_W / 2;
  sys[0] = LOGO_H / 2;
  for (int k = 1; k < N; k++) {
    sxs[k] = randF(rng, 0, LOGO_W);
    sys[k] = randF(rng, 0, LOGO_H);
  }
  // per shard: when it breaks loose, how it pops out, its lowest row
  float vx[N], vy[N], delay[N];
  int bottom[N];
  for (int k = 0; k < N; k++) {
    float dx = sxs[k] - LOGO_W / 2, dy = sys[k] - LOGO_H / 2, d = sqrtf(dx * dx + dy * dy) + 3;
    vx[k] = dx / d * randF(rng, 0.02f, 0.07f);
    vy[k] = dy / d * 0.03f - randF(rng, 0.02f, 0.07f);
    delay[k] = randF(rng, 0, 250);
    bottom[k] = 0;
  }
  static uint8_t owner[LOGO_H][LOGO_W];
  static uint8_t crack[LOGO_H][LOGO_W];
  for (int y = 0; y < LOGO_H; y++)
    for (int x = 0; x < LOGO_W; x++) {
      float d1 = 1e9f, d2 = 1e9f;
      int best = 0;
      for (int k = 0; k < N; k++) {
        float dx = x - sxs[k], dy = y - sys[k], d = dx * dx + dy * dy;
        if (d < d1) {
          d2 = d1;
          d1 = d;
          best = k;
        } else if (d < d2) {
          d2 = d;
        }
      }
      owner[y][x] = best;
      crack[y][x] = sqrtf(d2) - sqrtf(d1) < 1.0f;
      if (y > bottom[best]) bottom[best] = y;
    }

  float reach = tau < CRACK_MS ? (float)tau / CRACK_MS * 45 : 99;
  int shake = tau < CRACK_MS ? hashRand(seed, tau / 40, 4, 3) - 1 : 0;
  float a0 = tau > CRACK_MS ? (float)(tau - CRACK_MS) : -1;
  int floorY = H - 1 - LOGO_Y0;
  for (int y = 0; y < LOGO_H; y++)
    for (int x = 0; x < LOGO_W; x++) {
      int k = owner[y][x];
      uint32_t c = logoRgb(x, y);
      float dx = x - LOGO_W / 2, dy = y - LOGO_H / 2;
      bool cracked = crack[y][x] && dx * dx + dy * dy < reach * reach;
      int ox = shake, oy = 0;
      float a = a0 - delay[k];
      if (a > 0) {
        // falls until its lowest row is on the floor, then lies there
        float maxFall = (float)(floorY - bottom[k]);
        float fy = vy[k] * a + 0.5f * GRAV * 1.5f * a * a;
        float tLand = a;
        if (fy > maxFall) {
          fy = maxFall;
          // the time it landed: GRAV*0.75*t² + vy*t - maxFall = 0
          float A = GRAV * 0.75f, B = vy[k];
          tLand = (-B + sqrtf(B * B + 4 * A * maxFall)) / (2 * A);
          if (((tau / 90) + k * 7 + x) % 23 == 0) c = 0xFFFFFF;  // a glint
        }
        ox = (int)(vx[k] * tLand);
        oy = (int)fy;
        if (crack[y][x]) c = mixRgb(c, 0xFFFFFF, 140);  // the shard's edge
      } else if (cracked) {
        c = 0xFFFFFF;
      }
      if (c) g.drawPixel(LOGO_X0 + x + ox, LOGO_Y0 + y + oy, rgb565(c));
    }
}

// Schmelzen: flames underneath, the logo glows, then drips down in long
// strands into a puddle, steam rising.
static void breakMelt(Adafruit_GFX& g, uint32_t tau, uint32_t seed) {
  const uint32_t HEAT_MS = 500;
  int glow = tau < HEAT_MS ? (int)tau * 160 / HEAT_MS : 160;
  int puddle = 0;  // pixels of the logo that reached the floor
  for (int x = 0; x < LOGO_W; x++) {
    uint32_t rng = seed ^ (uint32_t)(x / 3 + 1) * 0x9E3779B9u;
    raceRand(rng);
    float delay = randF(rng, 0, 500) + (float)((x * 37) % 7) * 20;
    float sp = randF(rng, 0.01f, 0.03f);
    float a = (float)tau - HEAT_MS - delay;
    float drop = a > 0 ? sp * a + 0.00008f * a * a : 0;
    int prev = -999;
    for (int y = 0; y < LOGO_H; y++) {
      uint32_t c = logoRgb(x, y);
      int flick = hashRand(seed, tau / 60, x * 64 + y, 60);
      c = mixRgb(c ? c : 0x200000, (flick & 1) ? 0xFF6000 : 0xFF2000, glow * (y + 20) / (LOGO_H + 20));
      // the lower rows run further: long strands
      int yd = LOGO_Y0 + y + (int)(drop * (0.6f + 0.4f * y / LOGO_H) + drop * drop * 0.002f * y);
      if (yd > H - 1) {
        yd = H - 1;
        puddle++;
      }
      uint16_t c565 = rgb565(c);
      int from = prev == -999 || prev + 1 > yd ? yd : prev + 1;  // no gaps in the strand
      g.drawFastVLine(LOGO_X0 + x, from, yd - from + 1, c565);
      prev = yd;
    }
  }
  // the puddle spreads
  int pw = puddle / 12;
  if (pw > LOGO_W / 2 + 8) pw = LOGO_W / 2 + 8;
  if (pw > 0) {
    g.drawFastHLine(LOGO_CX - pw, H - 1, 2 * pw, rgb565(HSV_BLUE));
    g.drawFastHLine(LOGO_CX - pw * 2 / 3, H - 2, pw * 4 / 3, rgb565(0x8090FF));
  }
  // flames under it, while there is something to burn
  if (tau < 2500)
    for (int x = 0; x < LOGO_W; x += 1) {
      int h = 3 + hashRand(seed, tau / 70, x, 8) + (x % 5 == 0 ? 3 : 0);
      if (tau > 1800) h = h * (int)(2500 - tau) / 700;
      for (int j = 0; j < h; j++) {
        uint32_t c = j < h / 3 ? 0xFFE040 : (j < 2 * h / 3 ? 0xFF8000 : 0xD02000);
        g.drawPixel(LOGO_X0 + x, H - 1 - j, rgb565(c));
      }
    }
  // steam
  for (int i = 0; i < 14; i++) {
    uint32_t rng = seed ^ (i + 3) * 0x2545F491u;
    raceRand(rng);
    float x0 = randF(rng, 4, LOGO_W - 4), born = randF(rng, 300, 2500), life = randF(rng, 600, 1100);
    float s = (float)tau - born;
    if (s < 0 || s >= life) continue;
    int x = LOGO_X0 + (int)(x0 + 3 * sinf(s / 120 + i)), y = LOGO_Y0 + 6 - (int)(s * 0.03f);
    g.drawPixel(x, y, rgb565(s < life / 2 ? 0x909090 : 0x484848));
  }
}

// Kanonade: one ball isn't enough, a dozen more punch holes into the logo,
// each hit knocks it about, then the riddled rest falls off the board.
static void breakCannon(Adafruit_GFX& g, uint32_t tau, uint32_t seed) {
  const int N = 12;
  const uint32_t EVERY_MS = 110, FLIGHT_MS = 300;
  const uint32_t DROP_MS = (N - 1) * EVERY_MS + FLIGHT_MS + 150;
  int hx[N], hy[N], hr[N];
  uint32_t rng = seed;
  raceRand(rng);
  for (int k = 0; k < N; k++) {
    hx[k] = k ? 5 + (int)(raceRand(rng) % (LOGO_W - 10)) : LOGO_W / 2;
    hy[k] = k ? 4 + (int)(raceRand(rng) % (LOGO_H - 8)) : LOGO_H / 2;
    hr[k] = 3 + (int)(raceRand(rng) % 3);
  }
  auto hitMs = [&](int k) { return k ? (k - 1) * EVERY_MS + FLIGHT_MS : 0; };

  // each hit knocks the logo, decaying until the next
  int kx = 0, ky = 0, hits = 0;
  for (int k = 0; k < N; k++)
    if (tau >= hitMs(k)) hits++;
  uint32_t since = tau - hitMs(hits - 1);
  if (since < 100) {
    kx = -3 + (int)since * 3 / 100 - 1;
    ky = (hits % 2 ? 2 : -2) * (100 - (int)since) / 100;
  }
  kx -= hits / 3;  // pushed back further and further
  float a = tau > DROP_MS ? (float)(tau - DROP_MS) : 0;
  int dropY = (int)(0.5f * GRAV * 2 * a * a), dropX = (int)(-a * 0.02f);
  float tilt = a * 0.002f;

  for (int y = 0; y < LOGO_H; y++)
    for (int x = 0; x < LOGO_W; x++) {
      bool hole = false, rim = false;
      for (int k = 0; k < hits && !hole; k++) {
        int dx = x - hx[k], dy = y - hy[k], d = dx * dx + dy * dy;
        if (d <= hr[k] * hr[k]) hole = true;
        else if (d <= (hr[k] + 1) * (hr[k] + 1)) rim = true;
      }
      if (hole) continue;
      uint32_t c = rim ? 0x803010 : logoRgb(x, y);  // scorched rims
      if (c) g.drawPixel(LOGO_X0 + x + kx + dropX + (int)(tilt * y), LOGO_Y0 + y + ky + dropY + (int)(tilt * x * 0.5f),
                         rgb565(c));
    }

  for (int k = 1; k < N; k++) {
    int32_t s = (int32_t)tau - (int32_t)((k - 1) * EVERY_MS);
    int tx = LOGO_X0 + hx[k] + kx, ty = LOGO_Y0 + hy[k] + ky;
    if (s >= 0 && s < (int32_t)FLIGHT_MS) drawBallFlight(g, tx, ty, s * 100 / FLIGHT_MS);
    // splinters off each hit
    int32_t h = (int32_t)tau - (int32_t)hitMs(k);
    if (h >= 0 && h < 400)
      for (int i = 0; i < 8; i++) {
        float ang = 0.785f * i + k, d = h * 0.06f;
        g.drawPixel(tx + (int)(cosf(ang) * d), ty + (int)(sinf(ang) * d + 0.5f * GRAV * h * h),
                    rgb565(i % 2 ? 0xFFFFFF : HSV_BLUE));
      }
  }
}

// Abfluss: the logo swirls round and round, ever smaller, down the drain, plop.
static void breakDrain(Adafruit_GFX& g, uint32_t tau, uint32_t seed) {
  const uint32_t SWIRL_MS = 1300;
  const float R = 36;
  if (tau < SWIRL_MS) {
    float f = (float)tau / SWIRL_MS;
    float scale = 1 - f * f * 0.97f;
    float turn = 14 * f * f * (seed & 1 ? 1 : -1);
    int reach = (int)(R * scale) + 1;
    for (int y = -reach; y <= reach; y++)
      for (int x = -reach; x <= reach; x++) {
        float r = sqrtf((float)(x * x + y * y));
        if (r > R) continue;
        // the middle turns faster than the rim
        float ang = atan2f((float)y, (float)x) - turn * (1.2f - r / R * scale);
        float rs = r / scale;
        int lx = (int)(cosf(ang) * rs) + LOGO_W / 2, ly = (int)(sinf(ang) * rs) + LOGO_H / 2;
        if (lx < 0 || lx >= LOGO_W || ly < 0 || ly >= LOGO_H) continue;
        uint32_t c = logoRgb(lx, ly);
        if (c) g.drawPixel(LOGO_CX + x, LOGO_CY + y, rgb565(c));
      }
    // the drain itself
    g.fillCircle(LOGO_CX, LOGO_CY, 1, rgb565(0x404040));
  } else if (tau < SWIRL_MS + 400) {
    // plop: a ring and a few drops
    int s = (int)(tau - SWIRL_MS);
    g.drawCircle(LOGO_CX, LOGO_CY, 1 + s / 25, rgb565(s < 200 ? 0xFFFFFF : 0x6080FF));
    for (int i = 0; i < 6; i++) {
      float ang = 1.047f * i + 0.5f, d = s * 0.04f;
      g.drawPixel(LOGO_CX + (int)(cosf(ang) * d), LOGO_CY + (int)(sinf(ang) * d - s * 0.05f + 0.5f * GRAV * s * s),
                  rgb565(HSV_BLUE));
    }
  }
}

void drawConceded(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Match& m, uint32_t t, uint32_t seed) {
  u8.setFontMode(1);
  int anim = (int)(seed % CONCEDED_ANIMS);

  // the ball comes in from the right in an arc and goes into the Raute
  if (t < FLY_MS) {
    drawHsvLogo(g, LOGO_CX, LOGO_CY);
    drawBallFlight(g, LOGO_CX, LOGO_CY, (int)(t * 100 / FLY_MS));
    return;
  }
  uint32_t tau = t - FLY_MS, textMs = TEXT_MS[anim];
  if (tau >= textMs) drawBursts(g, tau - textMs, 0, TEXT_X0, W);
  switch (anim) {
    case BREAK_BOOM: breakBoom(g, tau, seed); break;
    case BREAK_TEAR: breakTear(g, tau, seed); break;
    case BREAK_SHATTER: breakShatter(g, tau, seed); break;
    case BREAK_MELT: breakMelt(g, tau, seed); break;
    case BREAK_CANNON: breakCannon(g, tau, seed); break;
    default: breakDrain(g, tau, seed); break;
  }
  if (tau < textMs) return;

  int textCx = (TEXT_X0 + W) / 2;
  bool score = tau >= textMs + SCORE_AFTER_MS && m.id;  // a test keeps the text in the middle
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
  u8.setForegroundColor(rgb565(GOAL_RGB[((tau - textMs) / BLINK_MS) % 3]));
  u8.drawUTF8(textCx - kw / 2, kBase, "KASSIERT!");

  if (score) drawScore(g, u8, m, "", textCx, W - TEXT_X0 - 4);
}
