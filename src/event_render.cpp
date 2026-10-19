#include "event_render.h"

#include <math.h>

#include "config.h"

static const int W = PANEL_RES_X * PANEL_CHAIN;
static const int H = PANEL_RES_Y;
static const int MARGIN = 2;  // to the board's edges

// TextSize order; all with umlauts (_tf)
static const uint8_t* const FONTS[] = {
    u8g2_font_helvB08_tf,    // Small: as the header
    u8g2_font_t0_13b_tf,     // Normal: as a departure row
    u8g2_font_helvB18_tf,    // Large: about two rows
    u8g2_font_logisoso38_tf, // Huge: most of the board
};
static const int WAVE_PX[] = {1, 2, 3, 4};  // TextSize order

// Too long texts: wait, scroll to the end, wait, jump back (as the
// destinations in src/render.cpp).
static const uint32_t PAUSE_MS = 2500;
static const int SPEED_PX_S = 20;

// Typewriter: the whole text stays this long before it starts again
static const uint32_t TYPE_HOLD_MS = 2500;

static int pingPongOffset(int overflow, uint32_t ms) {
  uint32_t scrollMs = (uint32_t)overflow * 1000 / SPEED_PX_S;
  uint32_t t = ms % (PAUSE_MS + scrollMs + PAUSE_MS);
  if (t < PAUSE_MS) return 0;
  t -= PAUSE_MS;
  return t < scrollMs ? (int)(t * SPEED_PX_S / 1000) : overflow;
}

// Next character of UTF-8 `p` (the texts are sanitized: at most 3 bytes).
static uint16_t nextChar(const char*& p) {
  uint8_t c = *p++;
  if (c < 0x80) return c;
  int more = c >= 0xE0 ? 2 : 1;
  uint16_t u = c & (more == 2 ? 0x0F : 0x1F);
  while (more-- && (*p & 0xC0) == 0x80) u = u << 6 | (*p++ & 0x3F);
  return u;
}

static int charCount(const char* s) {
  int n = 0;
  while (*s) nextChar(s), n++;
  return n;
}

// Draws one line `ms` after its scene came up: placed by size, position and
// alignment, then character by character for the colour and animation.
static void drawLine(U8G2_FOR_ADAFRUIT_GFX& u8, const EventLine& l, uint32_t ms) {
  if (!l.text[0]) return;
  const int sp = l.speed;  // 1..10, 5 = normal
  if (l.anim == Anim::Blink && ms % (5000 / sp) >= 2500 / sp) return;

  u8.setFont(FONTS[(int)l.size]);
  u8.setFontMode(1);  // transparent, so the background stays; setFont() resets it
  int ascent = u8.getFontAscent();
  int descent = -u8.getFontDescent();  // counts the baseline row
  int base;
  switch (l.v) {
    case VPos::Top: base = MARGIN + ascent; break;
    case VPos::Bottom: base = H - MARGIN - descent; break;
    default: base = (H + ascent) / 2; break;
  }
  int tw = u8.getUTF8Width(l.text);
  int box = W - 2 * MARGIN;
  int x;
  if (l.anim == Anim::Scroll) x = W - (int)((uint64_t)ms * (10 + 4 * sp) / 1000 % (tw + W));
  else if (tw > box) x = MARGIN - pingPongOffset(tw - box, ms);
  else if (l.h == HAlign::Left) x = MARGIN;
  else if (l.h == HAlign::Right) x = W - MARGIN - tw;
  else x = (W - tw) / 2;

  int shown = 1 << 30;  // characters drawn (typewriter)
  if (l.anim == Anim::Type) {
    int n = charCount(l.text);
    uint32_t perChar = 500 / sp;  // 5: 10 characters per second
    uint32_t t = ms % (n * perChar + TYPE_HOLD_MS);
    shown = (int)(t / perChar);
  }
  int level = 256;
  if (l.anim == Anim::Pulse) level = 77 + (int)(179 * (0.5f + 0.5f * cosf(2 * (float)M_PI * ms * sp / 8000)));
  uint32_t color = dimColor(l.color, level);

  const char* p = l.text;
  for (int i = 0; *p && i < shown; i++) {
    uint16_t c = nextChar(p);
    if (l.rainbow) color = dimColor(hueColor(i * 24 + (int)((uint64_t)ms * 36 * sp / 1000)), level);
    int dy = 0;
    if (l.anim == Anim::Wave)
      dy = (int)lroundf(WAVE_PX[(int)l.size] * sinf(2 * (float)M_PI * ms * sp / 6000 - i * 0.6f));
    u8.setForegroundColor(rgb565(color));
    x += u8.drawGlyph(x, base + dy, c);
    if (x >= W + 50) break;  // the rest is off the board
  }
}

void renderScene(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Scene& sc, uint32_t ms) {
  if (sc.bg) g.fillScreen(rgb565(sc.bg));
  for (int i = 0; i < sc.nLines; i++) drawLine(u8, sc.lines[i], ms);
}

// Draws onto `out` shifted by `dx` and only within the columns [x0, x1):
// for the transitions, which show two scenes side by side.
struct Window : Adafruit_GFX {
  Adafruit_GFX& out;
  int dx, x0, x1;
  Window(Adafruit_GFX& out, int dx, int x0, int x1) : Adafruit_GFX(W, H), out(out), dx(dx), x0(x0), x1(x1) {}
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    x += dx;
    if (x >= x0 && x < x1) out.drawPixel(x, y, c);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override {
    int a = x + dx > x0 ? x + dx : x0, b = x + dx + w < x1 ? x + dx + w : x1;
    if (a < b) out.drawFastHLine(a, y, b - a, c);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override {
    x += dx;
    if (x >= x0 && x < x1) out.drawFastVLine(x, y, h, c);
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) override {
    for (int i = 0; i < h; i++) drawFastHLine(x, y + i, w, c);
  }
};

static void renderSceneIn(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Scene& sc, uint32_t ms, int dx,
                          int x0, int x1) {
  Window win(g, dx, x0, x1);
  u8.begin(win);
  renderScene(win, u8, sc, ms);
  u8.begin(g);
}

void renderEvent(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Event& e, uint32_t ms) {
  uint32_t sceneMs;
  int i = eventSceneAt(e, ms, sceneMs);
  if (i < 0) return;
  const Scene& sc = e.scenes[i];
  int prev = (i + e.nScenes - 1) % e.nScenes;
  const Scene& ps = e.scenes[prev];
  // the first TRANSITION_MS of a scene hand over from the previous one (not
  // at the very start, nor with a single scene)
  if (e.nScenes < 2 || ms < TRANSITION_MS || sceneMs >= TRANSITION_MS || ps.trans == Transition::Cut) {
    renderScene(g, u8, sc, sceneMs);
    return;
  }
  int edge = (int)(sceneMs * W / TRANSITION_MS);  // 0 .. W
  uint32_t prevMs = ps.durS * 1000UL + sceneMs;  // the previous scene goes on moving meanwhile
  if (ps.trans == Transition::Slide) {
    renderSceneIn(g, u8, ps, prevMs, -edge, 0, W);
    renderSceneIn(g, u8, sc, sceneMs, W - edge, 0, W);
  } else {  // Wipe: the new scene uncovers from the left
    renderSceneIn(g, u8, ps, prevMs, 0, edge, W);
    renderSceneIn(g, u8, sc, sceneMs, 0, 0, edge);
  }
}
