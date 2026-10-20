#include "karaoke_render.h"

#include <stdio.h>

#include "config.h"
#include "event.h"
#include "ping_pong.h"

// As the departures (src/render.cpp): heading, rule, four rows
static const int W = PANEL_RES_X * PANEL_CHAIN;
static const int H = PANEL_RES_Y;
static const int MARGIN_X = 4;
static const int HEADER_BASE = 10;
static const int RULE_Y = 12;
static const int ROW_BASE0 = 24;
static const int ROW_PITCH = 12;
static const int COL_GAP = 8;       // between name and song
static const int NAME_MAX_W = 72;   // longer names move to and fro in their column
static const int SPEED_PX_S = 13;   // as the destinations

#define FONT_HEADER u8g2_font_helvB08_tf
#define FONT_ROW    u8g2_font_t0_13b_tf

// Passes on only what falls within the columns [x0, x1).
struct ClipX : Adafruit_GFX {
  Adafruit_GFX& out;
  int x0, x1;
  ClipX(Adafruit_GFX& out, int x0, int x1) : Adafruit_GFX(W, H), out(out), x0(x0), x1(x1) {}
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (x >= x0 && x < x1) out.drawPixel(x, y, c);
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) override {
    int a = x > x0 ? x : x0, b = x + w < x1 ? x + w : x1;
    if (a < b) out.drawFastHLine(a, y, b - a, c);
  }
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) override {
    if (x >= x0 && x < x1) out.drawFastVLine(x, y, h, c);
  }
};

// `s` in the columns [x0, x1), moving to and fro if it is wider.
static void drawIn(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const char* s, int x0, int x1, int base,
                   uint32_t ms) {
  int tw = u8.getUTF8Width(s);
  if (tw <= x1 - x0) {
    u8.drawUTF8(x0, base, s);
    return;
  }
  ClipX clip(g, x0, x1);
  u8.begin(clip);
  u8.drawUTF8(x0 - pingPongOffset(tw - (x1 - x0), SPEED_PX_S, ms), base, s);
  u8.begin(g);
}

void renderKaraoke(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Karaoke& k, uint32_t ms) {
  u8.setFontMode(1);
  u8.setFont(FONT_HEADER);
  uint16_t titleColor = rgb565(k.titleColor);
  u8.setForegroundColor(titleColor);
  int titleEnd = W - MARGIN_X;
  int waiting = k.n - KARAOKE_ROWS;
  if (waiting > 0) {  // how many come after those shown
    char more[16];
    snprintf(more, sizeof(more), "+%d weitere", waiting);
    int x = W - MARGIN_X - u8.getUTF8Width(more);
    u8.drawUTF8(x, HEADER_BASE, more);
    titleEnd = x - COL_GAP;
  }
  drawIn(g, u8, k.title, MARGIN_X, titleEnd, HEADER_BASE, ms);
  g.drawFastHLine(MARGIN_X, RULE_Y, W - 2 * MARGIN_X, titleColor);

  u8.setFont(FONT_ROW);
  int rows = k.n < KARAOKE_ROWS ? k.n : KARAOKE_ROWS;
  if (rows == 0) {
    const char* s = "Wer singt als Erstes?";
    u8.setForegroundColor(rgb565(k.colors[0]));
    u8.drawUTF8((W - u8.getUTF8Width(s)) / 2, (RULE_Y + H) / 2 + u8.getFontAscent() / 2, s);
    return;
  }
  // one column for the names, as wide as the widest shown (up to NAME_MAX_W)
  int nameW = 0;
  for (int i = 0; i < rows; i++) {
    int w = u8.getUTF8Width(k.entries[i].name);
    if (w > nameW) nameW = w;
  }
  if (nameW > NAME_MAX_W) nameW = NAME_MAX_W;
  int songX = MARGIN_X + nameW + COL_GAP;
  for (int i = 0; i < rows; i++) {
    const KaraokeEntry& e = k.entries[i];
    int base = ROW_BASE0 + i * ROW_PITCH;
    u8.setForegroundColor(rgb565(k.colors[i]));
    drawIn(g, u8, e.name, MARGIN_X, e.song[0] ? MARGIN_X + nameW : W - MARGIN_X, base, ms);
    if (e.song[0]) drawIn(g, u8, e.song, songX, W - MARGIN_X, base, ms);
  }
}
