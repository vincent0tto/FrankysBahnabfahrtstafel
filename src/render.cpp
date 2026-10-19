#include "render.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "depart_anim.h"
#include "icons.h"
#include "parse.h"

// ---- Layout (192 x 64) ------------------------------------------------------
// y values are text baselines.
static const int W = PANEL_RES_X * PANEL_CHAIN;
static const int H = PANEL_RES_Y;
static const int MARGIN_X = 4;           // left/right edge
static const int HEADER_BASE = 10;       // small font
static const int RULE_Y = 12;
static const int ROW_BASE0 = 24;         // first departure row
static const int ROW_PITCH = 12;
static const int MAX_ROWS = 3;           // last row is replaced by the ticker
static const int COL_GAP = 6;            // between line / destination / time

#define FONT_HEADER u8g2_font_helvB08_tf
#define FONT_ROW    u8g2_font_t0_13b_tf

// Destination scrolling: wait, scroll to the end, wait, jump back.
static const uint32_t DEST_PAUSE_MS = 2500;
static const int DEST_SPEED_PX_S = 13;

static int ROW_ASCENT, ROW_DESCENT;  // set per frame from the font

static int width(U8G2_FOR_ADAFRUIT_GFX& u8, const char* s) { return u8.getUTF8Width(s); }

// Draws nothing, only records which columns a text really covers: glyphs sit
// differently in their advance width ("1" vs "A"), so centring by width alone
// doesn't line them up.
struct InkBounds : Adafruit_GFX {
  int minX = 32767, maxX = -32768;
  InkBounds() : Adafruit_GFX(32767, 32767) {}
  void drawPixel(int16_t x, int16_t, uint16_t) override {
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
  }
};

// Twice the x of the ink centre of `s` drawn at x = 0 (doubled: no rounding).
static int inkCenter2(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const char* s) {
  InkBounds ink;
  u8.begin(ink);
  u8.drawUTF8(0, 100, s);
  u8.begin(g);
  return ink.maxX >= ink.minX ? ink.minX + ink.maxX : width(u8, s) - 1;
}

static void centered(U8G2_FOR_ADAFRUIT_GFX& u8, int base, const char* s) {
  u8.drawUTF8((W - width(u8, s)) / 2, base, s);
}

static int rowBase(int i) { return ROW_BASE0 + i * ROW_PITCH; }

// A row's horizontal slice. Slices of neighbouring rows touch but never overlap.
// u8g2's descent counts the baseline row, so the lowest pixel is base + descent - 1.
static int rowBottom(int base) { return base + ROW_DESCENT - 1; }
static int rowTop(int base) { return base == ROW_BASE0 ? RULE_Y + 1 : rowBottom(base - ROW_PITCH) + 1; }

// Black out a row's slice outside [x0, x1) -> poor man's clipping. Glyph parts
// taller than the reported ascent (umlaut dots etc.) are cut too.
static void clipRow(Adafruit_GFX& g, int base, int x0, int x1) {
  int top = rowTop(base);
  int h = rowBottom(base) + 1 - top;
  if (x0 > 0) g.fillRect(0, top, x0, h, 0);
  if (x1 < W) g.fillRect(x1, top, W - x1, h, 0);
}

// Pixel offset for a text that is `overflow` px wider than its box.
static int pingPongOffset(int overflow, uint32_t ms) {
  uint32_t scrollMs = (uint32_t)overflow * 1000 / DEST_SPEED_PX_S;
  uint32_t period = DEST_PAUSE_MS + scrollMs + DEST_PAUSE_MS;
  uint32_t t = ms % period;
  if (t < DEST_PAUSE_MS) return 0;
  t -= DEST_PAUSE_MS;
  if (t < scrollMs) return (int)(t * DEST_SPEED_PX_S / 1000);
  return overflow;
}

// ---- Departure animation -----------------------------------------------------
static int animChance = DEPART_ANIM_CHANCE;
void setDepartAnimChance(int oneIn) { animChance = oneIn; }

static bool sameTrip(const Departure& a, const Departure& b) {
  return a.planned == b.planned && strcmp(a.line, b.line) == 0 && strcmp(a.dest, b.dest) == 0;
}

static Departure prevRows[MAX_ROWS + 1];  // rows shown in the previous frame
static int prevN = 0;
static struct {
  bool active = false;
  Departure dep;
  int row;
  uint32_t startMs;
} anim;

// Rolls the dice when a row shown last frame has just left (its minute is
// over). Works whether the departure is still in the board or a fetch already
// dropped it. Only if it was the only one leaving that minute: with several
// rows on "sofort" a single vehicle driving off would look odd. One roll per
// departure moment, one animation at a time.
static void checkDeparted(const Departure* const* rows, int n, time_t now, uint32_t ms) {
  if (!anim.active) {
    int left = 0, row = -1;
    for (int j = 0; j < prevN; j++) {
      const Departure& d = prevRows[j];
      if (d.cancelled || minutesUntil(d.when, now) != -1) continue;
      bool stillShown = false;
      for (int i = 0; i < n; i++) stillShown |= sameTrip(*rows[i], d);
      if (stillShown) continue;
      left++;
      row = j;
    }
    // Buses are rare here, so they get their own (higher) chance
    int chance = left == 1 && animChance > 0 && prevRows[row].cls > 4 ? DEPART_ANIM_CHANCE_BUS : animChance;
    if (left == 1 && chance > 0 && rand() % chance == 0) {
      anim.active = true;
      anim.dep = prevRows[row];
      anim.row = row;
      anim.startMs = ms;
    }
  }
  prevN = n;
  for (int i = 0; i < n; i++) prevRows[i] = *rows[i];
}

// ---- Promo texts in the 4th row ---------------------------------------------
static const int ICON_GAP = 4;    // between an icon and a plain text
static const int RIGHT_GAP = 18;  // between the text and `right` in a scrolling promo

// An icon to draw: built-in (src/icons.cpp) or made on the web page.
struct IconRef {
  const PresetIcon* preset = nullptr;
  const CustomIcon* custom = nullptr;
  int w = 0, h = 0, drop = 0;
  explicit operator bool() const { return preset || custom; }
  bool on(int r, int c) const {
    return preset ? preset->rows[r][c] == '#' : (custom->rows[r] >> (ICON_W - 1 - c)) & 1;
  }
};

static IconRef findIcon(const Settings& s, const char* tag) {
  IconRef ic;
  int ci = customIconIndex(tag);
  if ((ic.preset = findPresetIcon(tag))) {
    ic.w = ic.preset->w;
    ic.h = ic.preset->h;
    ic.drop = ic.preset->drop;
  } else if (ci >= 0 && s.icons[ci].used) {
    ic.custom = &s.icons[ci];
    ic.w = ICON_W;
    ic.h = ICON_H;
  }
  return ic;
}

static void drawIcon(Adafruit_GFX& g, uint16_t color, const IconRef& ic, int x, int base) {
  int top = base - ic.h + ic.drop;
  for (int r = 0; r < ic.h; r++)
    for (int c = 0; c < ic.w; c++)
      if (ic.on(r, c)) g.drawPixel(x + c, top + r, color);
}

// Width of a departure-style promo's line column: its icon or text.
static int promoLineWidth(U8G2_FOR_ADAFRUIT_GFX& u8, const Settings& s, const Promo& p) {
  IconRef ic = findIcon(s, p.icon);
  return ic ? ic.w : width(u8, p.icon);
}

static bool isPromoDep(const Promo& p) { return p.right[0] != 0; }

// Width of a plain or scrolling promo: icon, text and (if `scroll`) `right`.
static int promoWidth(U8G2_FOR_ADAFRUIT_GFX& u8, const Settings& s, const Promo& p, bool scroll) {
  IconRef ic = findIcon(s, p.icon);
  int w = (ic ? ic.w + (p.text[0] ? ICON_GAP : 0) : 0) + width(u8, p.text);
  if (scroll && isPromoDep(p)) {
    if (!ic) w += width(u8, p.icon) + (p.icon[0] ? ICON_GAP : 0);
    w += RIGHT_GAP + width(u8, p.right);
  }
  return w;
}

// The 4th row now and then shows a promo or the VRN disruption text instead
// of its departure (with promoFullRow all the time while one is open, the
// promos in turn as scrolling text); with a disruption the two take turns.
static struct {
  uint32_t next = 0;   // ms of the next showing, 0 = not scheduled yet
  uint32_t start = 0;
  uint32_t durMs = 0;
  bool active = false;
  bool disruption = false;  // showing (or last showed) the disruption text
  bool scroll = false;      // the promo scrolls through once (promoFullRow)
  Promo item;               // copy, the settings may change meanwhile
} promo;
static char disruptBuf[TICKER_LEN + 16];  // copy, a fetch may change the board meanwhile

static int forcedPromo = -1;
static int promoCursor = -1;  // last promo shown with promoFullRow
void setPromoIndex(int idx) { forcedPromo = idx; }

static uint32_t promoPause(const Settings& s) {  // promoIntervalS +-50 %
  // without promos this still paces the disruption texts
  uint32_t base = (s.promoIntervalS ? s.promoIntervalS : 60) * 1000UL;
  return base / 2 + (uint32_t)rand() % (base + 1);
}

// A promo switched on and open at `minute` of `day`, or nullptr; with
// promoFullRow the next one in the list, else a random one.
static const Promo* pickPromo(const Settings& s, int minute, int32_t day) {
  if (s.promoIntervalS == 0 && !s.promoFullRow) return nullptr;
  int open[MAX_PROMOS], nOpen = 0;
  for (int i = 0; i < s.nPromos; i++)
    if (promoOpen(s.promos[i], minute, day)) open[nOpen++] = i;
  if (nOpen == 0) return nullptr;
  bool forced = forcedPromo >= 0 && forcedPromo < s.nPromos && promoOpen(s.promos[forcedPromo], minute, day);
  if (forced) return &s.promos[forcedPromo];
  if (!s.promoFullRow) return &s.promos[open[rand() % nOpen]];
  int k = 0;
  while (k < nOpen && open[k] <= promoCursor) k++;
  promoCursor = open[k < nOpen ? k : 0];
  return &s.promos[promoCursor];
}

// How long a scrolling text `tw` px wide stays: `passes` scroll passes,
// however long they take.
static uint32_t tickerMs(int tw, int passes) {
  return (uint32_t)passes * (tw + W - 2 * MARGIN_X) * 1000 / TICKER_SPEED_PX_S;
}

// Returns the text to show in the 4th row this frame, or nullptr; the
// disruption text if promo.disruption, else promo.item has the details. A
// new promo may start if `canStartPromo`, the disruption text (`disruption`,
// nullptr if none) if `canStartDisruption`. A running one stays its full
// time even if the departure gets close, unless `blocked` (the row is needed
// for a status message; the departure animation only keeps a new one from
// starting). With promoFullRow a promo needn't wait for the pause, only the
// disruption text does, and it scrolls through once instead of promoShowS.
// `minute`, `day`: local time for the promos' time windows and weekdays,
// minute -1 if the time isn't known.
static const char* promoText(U8G2_FOR_ADAFRUIT_GFX& u8, const Settings& s, bool canStartPromo, bool canStartDisruption, bool blocked,
                             const char* disruption, int disruptW, int minute, int32_t day, uint32_t ms) {
  if (promo.active && (blocked || ms - promo.start >= promo.durMs)) {
    promo.active = false;
    promo.next = ms + promoPause(s);
  }
  if (!promo.active) {
    if (!disruption) canStartDisruption = false;
    if (blocked || !(canStartPromo || canStartDisruption)) return nullptr;
    if (promo.next == 0) promo.next = ms + promoPause(s);
    bool due = (int32_t)(ms - promo.next) >= 0;
    if (!due && !s.promoFullRow) return nullptr;
    const Promo* p = nullptr;
    if (canStartPromo && !(due && canStartDisruption && !promo.disruption)) p = pickPromo(s, minute, day);
    if (p) {
      promo.item = *p;
      promo.disruption = false;
      promo.scroll = s.promoFullRow;
      promo.durMs = promo.scroll ? tickerMs(promoWidth(u8, s, *p, true), 1) : s.promoShowS * 1000UL;
    } else if (due && canStartDisruption) {
      strcpy(disruptBuf, disruption);
      promo.disruption = true;
      promo.durMs = tickerMs(disruptW, s.disruptPasses);
    } else {
      if (due) promo.next = ms + promoPause(s);
      return nullptr;
    }
    promo.active = true;
    promo.start = ms;
  }
  // a promo stays even if its time window closes meanwhile
  return promo.disruption ? disruptBuf : promo.item.text;
}

// Scrolls `text` through the 4th row from the right, `t` ms after it entered.
// A text that fits is centred instead, unless `alwaysScroll`.
static void drawTicker(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const char* text, uint32_t t,
                       bool alwaysScroll) {
  int base = rowBase(MAX_ROWS);
  int tw = width(u8, text);
  int box = W - 2 * MARGIN_X;
  if (tw <= box && !alwaysScroll) {
    centered(u8, base, text);
    return;
  }
  int span = tw + box;
  int offset = (int)((uint64_t)t * TICKER_SPEED_PX_S / 1000 % span);
  u8.drawUTF8(W - MARGIN_X - offset, base, text);
  clipRow(g, base, MARGIN_X, W - MARGIN_X);
}

// Draws a plain promo (optional icon in front), centred or ping-ponging; if
// `scroll` (also one with `right`, appended) scrolling in from the right like
// the ticker, `t` ms after it entered.
static void drawPromo(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Settings& s, int base,
                      const Promo& p, uint32_t ms, bool scroll, uint32_t t) {
  IconRef ic = findIcon(s, p.icon);
  int tw = promoWidth(u8, s, p, scroll);
  int box = W - 2 * MARGIN_X;
  int x;
  if (scroll) x = W - MARGIN_X - (int)((uint64_t)t * TICKER_SPEED_PX_S / 1000 % (tw + box));
  else x = tw <= box ? (W - tw) / 2 : MARGIN_X - pingPongOffset(tw - box, ms);
  if (ic) {
    drawIcon(g, color, ic, x, base);
    x += ic.w + (p.text[0] ? ICON_GAP : 0);
  } else if (scroll && isPromoDep(p) && p.icon[0]) {
    u8.drawUTF8(x, base, p.icon);
    x += width(u8, p.icon) + ICON_GAP;
  }
  u8.drawUTF8(x, base, p.text);
  if (scroll && isPromoDep(p)) u8.drawUTF8(x + width(u8, p.text) + RIGHT_GAP, base, p.right);
  if (scroll || tw > box) clipRow(g, base, MARGIN_X, W - MARGIN_X);
}

// Draws a promo with `right` in the departure columns: icon (or text) as
// line, text as destination, `right` as time.
static void drawPromoDep(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Settings& s, int base,
                         const Promo& p, int lineW, int destX, uint32_t ms) {
  int rightX = W - MARGIN_X - width(u8, p.right);
  int overflow = width(u8, p.text) - (rightX - COL_GAP - destX);
  int off = overflow > 0 ? pingPongOffset(overflow, ms) : 0;
  u8.drawUTF8(destX - off, base, p.text);
  clipRow(g, base, destX, rightX - COL_GAP);
  int lineX = MARGIN_X + lineW - promoLineWidth(u8, s, p);  // right-aligned like line numbers
  if (IconRef ic = findIcon(s, p.icon)) drawIcon(g, color, ic, lineX, base);
  else u8.drawUTF8(lineX, base, p.icon);
  u8.drawUTF8(rightX, base, p.right);
}

// `platforms`: the time column shows platforms, so its heading replaces the clock.
static void drawHeader(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const DisplayStatus& st,
                       time_t now, bool warn, bool platforms) {
  u8.setFont(FONT_HEADER);
  char clock[16] = "--:--";
  if (platforms) {
    snprintf(clock, sizeof(clock), "%s", PLATFORM_HEADER);
  } else if (st.timeValid) {
    struct tm lt;
    localtime_r(&now, &lt);
    snprintf(clock, sizeof(clock), "%02d:%02d", lt.tm_hour, lt.tm_min);
  }
  int clockX = W - MARGIN_X - width(u8, clock);
  int warnX = clockX - COL_GAP - width(u8, "!");
  int nameEnd = (warn ? warnX : clockX) - COL_GAP;

  u8.drawUTF8(MARGIN_X, HEADER_BASE, STOP_DISPLAY_NAME);
  g.fillRect(nameEnd, 0, W - nameEnd, RULE_Y, 0);  // cut a too long stop name
  if (warn) u8.drawUTF8(warnX, HEADER_BASE, "!");
  u8.drawUTF8(clockX, HEADER_BASE, clock);
  g.drawFastHLine(MARGIN_X, RULE_Y, W - 2 * MARGIN_X, color);
}

void renderFrame(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Board& b,
                 const Settings& s, const DisplayStatus& st, time_t now, uint32_t ms) {
  u8.setFontMode(1);
  u8.setForegroundColor(color);

  if (st.updating) {
    u8.setFont(FONT_ROW);
    centered(u8, (RULE_Y + H) / 2 + u8.getFontAscent() / 2, "Software-Update ...");
    drawHeader(g, u8, color, st, now, false, false);
    return;
  }

  long age = (st.timeValid && b.fetchedAt) ? (long)(now - b.fetchedAt) : -1;
  bool haveData = age >= 0 && age < STALE_DROP_S;
  bool stale = age >= STALE_WARN_S;

  u8.setFont(FONT_ROW);
  ROW_ASCENT = u8.getFontAscent();
  ROW_DESCENT = -u8.getFontDescent();

  // Bottom line: own status messages stay there for good (VRN disruption
  // texts take turns with the promos, see below)
  const char* tickerMsg = nullptr;
  if (!st.wifi) tickerMsg = "Keine WLAN-Verbindung";
  else if (stale && haveData) tickerMsg = "Daten nicht aktuell";
  int maxRows = tickerMsg ? MAX_ROWS : MAX_ROWS + 1;

  const Departure* rows[MAX_ROWS + 1];
  int n = 0;
  if (haveData)
    for (int i = 0; i < b.n && n < maxRows; i++)
      if (!hasDeparted(b.deps[i], now)) rows[n++] = &b.deps[i];

  // While the animation runs, the departed trip keeps its row. A running
  // promo / disruption keeps the 4th row: a trip hidden behind it doesn't
  // animate, and an animation stays in the rows above.
  int animMaxRows = promo.active ? MAX_ROWS : maxRows;
  checkDeparted(rows, n < animMaxRows ? n : animMaxRows, now, ms);
  int animRow = -1;
  if (anim.active && (!haveData || ms - anim.startMs >= DEPART_ANIM_MS)) anim.active = false;
  if (anim.active) {
    animRow = anim.row < n ? anim.row : n;
    if (animRow > animMaxRows - 1) animRow = animMaxRows - 1;
    if (n == maxRows) n--;  // the last row makes room
    for (int i = n; i > animRow; i--) rows[i] = rows[i - 1];
    rows[animRow] = &anim.dep;
    n++;
  }

  // Now and then a promo or the disruption texts replace the 4th row while
  // that departure is still a while away (the disruptions also if the row is
  // empty; with promoFullRow the promos regardless); once started a promo stays
  // promoShowS (with promoFullRow scrolls through once), the disruptions
  // scroll through disruptPasses times
  int minute = st.timeValid ? minuteOfDay(now) : -1;
  int32_t day = st.timeValid ? localDay(now) : 0;
  static char disruption[TICKER_LEN + 16];
  bool haveDisruption = haveData && b.ticker[0] && s.disruptOn;
  if (haveDisruption) snprintf(disruption, sizeof(disruption), "+++ %s +++", b.ticker);
  long rowMins = n == MAX_ROWS + 1 ? minutesUntil(rows[MAX_ROWS]->when, now) : -1;
  const char* promoMsg = promoText(u8, s, s.promoFullRow || rowMins >= PROMO_MIN_MINUTES,
                                   n <= MAX_ROWS || rowMins >= DISRUPTION_MIN_MINUTES,
                                   tickerMsg || animRow == MAX_ROWS,
                                   haveDisruption ? disruption : nullptr,
                                   haveDisruption ? width(u8, disruption) : 0, minute, day, ms);
  bool showDisruption = promoMsg && promo.disruption;
  const int nLineW = n;  // the hidden 4th row still reserves its line width
  if (promoMsg && n > MAX_ROWS) n = MAX_ROWS;
  bool promoIsDep = promoMsg && !showDisruption && !promo.scroll && isPromoDep(promo.item);

  // Now and then the time column shows the platforms instead (also for
  // cancelled trips, not without a platform)
  const uint32_t platformEveryMs = PLATFORM_EVERY_S * 1000UL;
  bool platformPhase = platformEveryMs > 0 &&
                       ms % (platformEveryMs ? platformEveryMs : 1) >= platformEveryMs - PLATFORM_SHOW_S * 1000UL;
  bool showPlatforms = false;
  for (int i = 0; i < n && platformPhase; i++)
    showPlatforms |= rows[i]->platformLabel[0] != 0;

  if (n == 0) {
    int mid = (RULE_Y + (tickerMsg || showDisruption ? rowBase(MAX_ROWS) - ROW_ASCENT : H)) / 2;
    if (!st.timeValid || (b.fetchedAt == 0 && st.wifi)) {
      centered(u8, mid + ROW_ASCENT / 2, st.wifi ? "Lade Abfahrten ..." : "Verbinde mit WLAN ...");
    } else {
      centered(u8, mid - 2, "Zurzeit keine");
      centered(u8, mid + ROW_ASCENT + 1, "Abfahrtsinformationen");
    }
  } else {
    int lineW = width(u8, "88");
    for (int i = 0; i < nLineW; i++) {
      int w = width(u8, rows[i]->line);
      if (w > lineW) lineW = w;
    }
    if (promoIsDep && promoLineWidth(u8, s, promo.item) > lineW) lineW = promoLineWidth(u8, s, promo.item);
    const int destX = MARGIN_X + lineW + COL_GAP;
    // Platforms are centred (by their ink) under the header's PLATFORM_HEADER
    u8.setFont(FONT_HEADER);
    int headerX = W - MARGIN_X - width(u8, PLATFORM_HEADER);  // as in drawHeader
    int platformCenter2 = 2 * headerX + inkCenter2(g, u8, PLATFORM_HEADER);
    u8.setFont(FONT_ROW);
    char right[MAX_ROWS + 1][16];
    int rightX[MAX_ROWS + 1], destEnd[MAX_ROWS + 1];
    // 1) destinations (possibly scrolled), 2) clip all rows, 3) line + time
    for (int i = 0; i < n; i++) {
      formatCountdown(*rows[i], now, right[i], sizeof(right[i]));
      rightX[i] = W - MARGIN_X - width(u8, right[i]);
      // the destination keeps the width it has next to the countdown
      destEnd[i] = rightX[i] - COL_GAP;
      if (showPlatforms && rows[i]->platformLabel[0]) {
        snprintf(right[i], sizeof(right[i]), "%s", rows[i]->platformLabel);
        int x = (platformCenter2 - inkCenter2(g, u8, right[i])) / 2;
        int maxX = W - MARGIN_X - width(u8, right[i]);  // a long label stays on the board
        rightX[i] = x < maxX ? x : maxX;
        if (rightX[i] - COL_GAP < destEnd[i]) destEnd[i] = rightX[i] - COL_GAP;
      }
      int overflow = width(u8, rows[i]->dest) - (destEnd[i] - destX);
      int off = overflow > 0 ? pingPongOffset(overflow, ms) : 0;
      u8.drawUTF8(destX - off, rowBase(i), rows[i]->dest);
    }
    for (int i = 0; i < n; i++) clipRow(g, rowBase(i), destX, destEnd[i]);
    for (int i = 0; i < n; i++) {
      int base = rowBase(i);
      u8.drawUTF8(MARGIN_X + lineW - width(u8, rows[i]->line), base, rows[i]->line);  // right-aligned
      u8.drawUTF8(rightX[i], base, right[i]);
    }
    if (animRow >= 0) {
      int base = rowBase(animRow);
      drawDepartAnim(g, color, anim.dep.cls <= 4, ms - anim.startMs, rowTop(base), rowBottom(base), W);
    }
    if (promoIsDep) drawPromoDep(g, u8, color, s, rowBase(MAX_ROWS), promo.item, lineW, destX, ms);
  }
  if (showDisruption) drawTicker(g, u8, promoMsg, ms - promo.start, true);
  else if (promoMsg && !promoIsDep)
    drawPromo(g, u8, color, s, rowBase(MAX_ROWS), promo.item, ms, promo.scroll, ms - promo.start);

  if (tickerMsg) drawTicker(g, u8, tickerMsg, ms, false);

  drawHeader(g, u8, color, st, now, stale || !st.wifi, showPlatforms);
}
