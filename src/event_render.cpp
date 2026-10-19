#include "event_render.h"

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

// Too long texts: wait, scroll to the end, wait, jump back (as the
// destinations in src/render.cpp).
static const uint32_t PAUSE_MS = 2500;
static const int SPEED_PX_S = 20;

static int pingPongOffset(int overflow, uint32_t ms) {
  uint32_t scrollMs = (uint32_t)overflow * 1000 / SPEED_PX_S;
  uint32_t t = ms % (PAUSE_MS + scrollMs + PAUSE_MS);
  if (t < PAUSE_MS) return 0;
  t -= PAUSE_MS;
  return t < scrollMs ? (int)(t * SPEED_PX_S / 1000) : overflow;
}

static void drawLine(U8G2_FOR_ADAFRUIT_GFX& u8, const EventLine& l, uint32_t ms) {
  if (!l.text[0]) return;
  u8.setFont(FONTS[(int)l.size]);
  u8.setFontMode(1);  // transparent, so the background stays; setFont() resets it
  u8.setForegroundColor(rgb565(l.color));
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
  if (tw > box) x = MARGIN - pingPongOffset(tw - box, ms);
  else if (l.h == HAlign::Left) x = MARGIN;
  else if (l.h == HAlign::Right) x = W - MARGIN - tw;
  else x = (W - tw) / 2;
  u8.drawUTF8(x, base, l.text);
}

void renderScene(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Scene& sc, uint32_t ms) {
  if (sc.bg) g.fillScreen(rgb565(sc.bg));
  for (int i = 0; i < sc.nLines; i++) drawLine(u8, sc.lines[i], ms);
}

void renderEvent(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Event& e, uint32_t ms) {
  uint32_t sceneMs;
  int i = eventSceneAt(e, ms, sceneMs);
  if (i >= 0) renderScene(g, u8, e.scenes[i], sceneMs);
}
