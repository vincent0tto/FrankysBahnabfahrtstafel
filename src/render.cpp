#include "render.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "depart_anim.h"
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
    if (left == 1 && animChance > 0 && rand() % animChance == 0) {
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
static const char* const PROMOS[] = PROMO_TEXTS;
static const int N_PROMOS = sizeof(PROMOS) / sizeof(PROMOS[0]);
static const char* const PROMO_IG = "[ig]";  // prefix -> Instagram logo

static const char* const IG_LOGO[] = {
    ".#########.",
    "#.........#",
    "#.......#.#",
    "#...###...#",
    "#..#...#..#",
    "#..#...#..#",
    "#..#...#..#",
    "#...###...#",
    "#.........#",
    "#.........#",
    ".#########.",
};
static const int IG_W = 11, IG_H = 11, IG_GAP = 4;

// Promo shown like a departure: "line|destination|time". A line from ICONS
// ("[gc]", ...) draws that icon instead of a line number.
struct Icon {
  const char* tag;
  const char* const* rows;
  int w, h;
};
static const char* const GC_ICON[] = {  // grilled cheese
    ".##########.",
    "#..#..#..#.#",
    "#.#..#..#..#",
    ".##########.",
    "############",
    "..#....#....",
    ".##########.",
    "#..........#",
    ".##########.",
};
static const char* const FW_ICON[] = {  // flat white
    "..#..#......",
    ".#..#.......",
    "########....",
    "#......####.",
    "#......#..#.",
    "#......####.",
    ".#....#.....",
    "..####......",
    "###########.",
};
static const char* const KU_ICON[] = {  // slice of cake
    "........##..",
    "........##..",
    "....########",
    "..##.......#",
    "############",
    "#..........#",
    "############",
    "#..........#",
    "############",
};
static const char* const BI_ICON[] = {  // beer mug
    ".##.##.##...",
    "#########...",
    "#.#.#.#.####",
    "#.#.#.#.#..#",
    "#.#.#.#.#..#",
    "#.#.#.#.#..#",
    "#.#.#.#.####",
    "#.#.#.#.#...",
    "#########...",
};
static const Icon ICONS[] = {
    {"[gc]", GC_ICON, 12, 9},
    {"[fw]", FW_ICON, 12, 9},
    {"[ku]", KU_ICON, 12, 9},
    {"[bi]", BI_ICON, 12, 9},
};

static const Icon* findIcon(const char* tag) {
  for (const Icon& ic : ICONS)
    if (strcmp(tag, ic.tag) == 0) return &ic;
  return nullptr;
}

struct PromoDep {
  char line[16], dest[64], time[16];
};

// Splits a "line|destination|time" promo, false for a plain text.
static bool splitPromoDep(const char* text, PromoDep& p) {
  const char* a = strchr(text, '|');
  const char* b = a ? strchr(a + 1, '|') : nullptr;
  if (!b) return false;
  snprintf(p.line, sizeof(p.line), "%.*s", (int)(a - text), text);
  snprintf(p.dest, sizeof(p.dest), "%.*s", (int)(b - a - 1), a + 1);
  snprintf(p.time, sizeof(p.time), "%s", b + 1);
  return true;
}

static int promoLineWidth(U8G2_FOR_ADAFRUIT_GFX& u8, const PromoDep& p) {
  const Icon* ic = findIcon(p.line);
  return ic ? ic->w : width(u8, p.line);
}

// Optional "@HH" or "@HH-HH" prefix: only from (until) that local hour.
// Returns the text after the prefix, nullptr if it isn't its time.
static const char* promoWindow(const char* text, int hour) {
  if (text[0] != '@' || !isdigit((unsigned char)text[1])) return text;
  char* end;
  long from = strtol(text + 1, &end, 10), to = 24;
  if (*end == '-') to = strtol(end + 1, &end, 10);
  if (hour < 0) return nullptr;  // no valid time yet
  bool open = from <= to ? hour >= from && hour < to : hour >= from || hour < to;
  return open ? end : nullptr;
}

static struct {
  uint32_t next = 0;   // ms of the next showing, 0 = not scheduled yet
  uint32_t start = 0;
  int idx = -1;        // -1 = not showing
} promo;

static int forcedPromo = -1;
void setPromoIndex(int idx) { forcedPromo = idx; }

static uint32_t promoPause() {  // PROMO_INTERVAL_S +-50 %
  uint32_t base = PROMO_INTERVAL_S * 1000UL;
  return base / 2 + (uint32_t)rand() % (base + 1);
}

// Returns the promo text to show in the 4th row this frame, or nullptr.
// `eligible`: the 4th row is free for it (its departure is far enough away).
// `hour`: local hour for "@HH" windows, -1 if the time isn't known.
static const char* promoText(bool eligible, int hour, uint32_t ms) {
  if (N_PROMOS == 0 || PROMO_INTERVAL_S <= 0) return nullptr;
  if (promo.idx >= 0 && ms - promo.start >= PROMO_SHOW_S * 1000UL) {
    promo.idx = -1;
    promo.next = ms + promoPause();
  }
  if (!eligible) {
    if (promo.idx >= 0) {  // a departure got close, give the row back
      promo.idx = -1;
      promo.next = ms + promoPause();
    }
    return nullptr;
  }
  if (promo.idx < 0) {
    if (promo.next == 0) promo.next = ms + promoPause();
    if ((int32_t)(ms - promo.next) < 0) return nullptr;
    int open[N_PROMOS > 0 ? N_PROMOS : 1], nOpen = 0;
    for (int i = 0; i < N_PROMOS; i++)
      if (promoWindow(PROMOS[i], hour)) open[nOpen++] = i;
    if (nOpen == 0) {
      promo.next = ms + promoPause();
      return nullptr;
    }
    promo.idx = forcedPromo >= 0 && promoWindow(PROMOS[forcedPromo], hour) ? forcedPromo
                                                                            : open[rand() % nOpen];
    promo.start = ms;
  }
  const char* text = promoWindow(PROMOS[promo.idx], hour);
  if (!text) {  // its window closed while showing
    promo.idx = -1;
    promo.next = ms + promoPause();
  }
  return text;
}

static void drawPromo(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, int base,
                      const char* text, uint32_t ms) {
  bool ig = strncmp(text, PROMO_IG, strlen(PROMO_IG)) == 0;
  if (ig) text += strlen(PROMO_IG);
  int iconW = ig ? IG_W + IG_GAP : 0;
  int tw = iconW + width(u8, text);
  int box = W - 2 * MARGIN_X;
  int x = tw <= box ? (W - tw) / 2 : MARGIN_X - pingPongOffset(tw - box, ms);
  if (ig) {
    int top = base - IG_H + 1;  // 1px below the letters' bottom, looks centred
    for (int r = 0; r < IG_H; r++)
      for (int c = 0; c < IG_W; c++)
        if (IG_LOGO[r][c] == '#') g.drawPixel(x + c, top + r, color);
  }
  u8.drawUTF8(x + iconW, base, text);
  if (tw > box) clipRow(g, base, MARGIN_X, W - MARGIN_X);
}

// Draws a "line|destination|time" promo in the departure columns.
static void drawPromoDep(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, int base,
                         const PromoDep& p, int lineW, int destX, uint32_t ms) {
  int rightX = W - MARGIN_X - width(u8, p.time);
  int overflow = width(u8, p.dest) - (rightX - COL_GAP - destX);
  int off = overflow > 0 ? pingPongOffset(overflow, ms) : 0;
  u8.drawUTF8(destX - off, base, p.dest);
  clipRow(g, base, destX, rightX - COL_GAP);
  int lineX = MARGIN_X + lineW - promoLineWidth(u8, p);  // right-aligned like line numbers
  if (const Icon* ic = findIcon(p.line)) {
    int top = base - ic->h;  // stands on the baseline like the digits
    for (int r = 0; r < ic->h; r++)
      for (int c = 0; c < ic->w; c++)
        if (ic->rows[r][c] == '#') g.drawPixel(lineX + c, top + r, color);
  } else {
    u8.drawUTF8(lineX, base, p.line);
  }
  u8.drawUTF8(rightX, base, p.time);
}

static void drawHeader(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const DisplayStatus& st,
                       time_t now, bool warn) {
  u8.setFont(FONT_HEADER);
  char clock[8] = "--:--";
  if (st.timeValid) {
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

static void centered(U8G2_FOR_ADAFRUIT_GFX& u8, int base, const char* s) {
  u8.drawUTF8((W - width(u8, s)) / 2, base, s);
}

void renderFrame(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, uint16_t color, const Board& b,
                 const DisplayStatus& st, time_t now, uint32_t ms) {
  u8.setFontMode(1);
  u8.setForegroundColor(color);

  long age = (st.timeValid && b.fetchedAt) ? (long)(now - b.fetchedAt) : -1;
  bool haveData = age >= 0 && age < STALE_DROP_S;
  bool stale = age >= STALE_WARN_S;

  drawHeader(g, u8, color, st, now, stale || !st.wifi);

  u8.setFont(FONT_ROW);
  ROW_ASCENT = u8.getFontAscent();
  ROW_DESCENT = -u8.getFontDescent();

  // Bottom line: own status messages win over VRN disruption texts
  const char* tickerMsg = nullptr;
  if (!st.wifi) tickerMsg = "Keine WLAN-Verbindung";
  else if (stale && haveData) tickerMsg = "Daten nicht aktuell";
  else if (haveData && b.ticker[0]) tickerMsg = b.ticker;
  int maxRows = tickerMsg ? MAX_ROWS : MAX_ROWS + 1;

  const Departure* rows[MAX_ROWS + 1];
  int n = 0;
  if (haveData)
    for (int i = 0; i < b.n && n < maxRows; i++)
      if (!hasDeparted(b.deps[i], now)) rows[n++] = &b.deps[i];

  // While the animation runs, the departed trip keeps its row
  checkDeparted(rows, n, now, ms);
  int animRow = -1;
  if (anim.active && (!haveData || ms - anim.startMs >= DEPART_ANIM_MS)) anim.active = false;
  if (anim.active) {
    animRow = anim.row < n ? anim.row : n;
    if (animRow > maxRows - 1) animRow = maxRows - 1;
    if (n == maxRows) n--;  // the last row makes room
    for (int i = n; i > animRow; i--) rows[i] = rows[i - 1];
    rows[animRow] = &anim.dep;
    n++;
  }

  // Now and then a promo text replaces the 4th row while that departure is
  // still a while away
  int hour = -1;
  if (st.timeValid) {
    struct tm lt;
    localtime_r(&now, &lt);
    hour = lt.tm_hour;
  }
  const char* promoMsg = promoText(n == MAX_ROWS + 1 && animRow != MAX_ROWS &&
                                       minutesUntil(rows[MAX_ROWS]->when, now) >= PROMO_MIN_MINUTES,
                                   hour, ms);
  const int nLineW = n;  // the hidden 4th row still reserves its line width
  if (promoMsg) n = MAX_ROWS;
  PromoDep promoDep;
  bool promoIsDep = promoMsg && splitPromoDep(promoMsg, promoDep);

  if (n == 0) {
    int mid = (RULE_Y + (tickerMsg ? rowBase(MAX_ROWS) - ROW_ASCENT : H)) / 2;
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
    if (promoIsDep && promoLineWidth(u8, promoDep) > lineW) lineW = promoLineWidth(u8, promoDep);
    const int destX = MARGIN_X + lineW + COL_GAP;
    char right[MAX_ROWS + 1][16];
    int rightX[MAX_ROWS + 1];
    // 1) destinations (possibly scrolled), 2) clip all rows, 3) line + time
    for (int i = 0; i < n; i++) {
      formatCountdown(*rows[i], now, right[i], sizeof(right[i]));
      rightX[i] = W - MARGIN_X - width(u8, right[i]);
      int overflow = width(u8, rows[i]->dest) - (rightX[i] - COL_GAP - destX);
      int off = overflow > 0 ? pingPongOffset(overflow, ms) : 0;
      u8.drawUTF8(destX - off, rowBase(i), rows[i]->dest);
    }
    for (int i = 0; i < n; i++) clipRow(g, rowBase(i), destX, rightX[i] - COL_GAP);
    for (int i = 0; i < n; i++) {
      int base = rowBase(i);
      u8.drawUTF8(MARGIN_X + lineW - width(u8, rows[i]->line), base, rows[i]->line);  // right-aligned
      u8.drawUTF8(rightX[i], base, right[i]);
    }
    if (animRow >= 0) {
      int base = rowBase(animRow);
      drawDepartAnim(g, color, anim.dep.cls <= 4, ms - anim.startMs, rowTop(base), rowBottom(base), W);
    }
    if (promoIsDep) drawPromoDep(g, u8, color, rowBase(MAX_ROWS), promoDep, lineW, destX, ms);
  }
  if (promoMsg && !promoIsDep) drawPromo(g, u8, color, rowBase(MAX_ROWS), promoMsg, ms);

  if (tickerMsg) {
    int base = rowBase(MAX_ROWS);
    int tw = width(u8, tickerMsg);
    int box = W - 2 * MARGIN_X;
    if (tw <= box) {
      centered(u8, base, tickerMsg);
    } else {
      int span = tw + box;
      int offset = (int)((uint64_t)ms * TICKER_SPEED_PX_S / 1000 % span);
      u8.drawUTF8(W - MARGIN_X - offset, base, tickerMsg);
      clipRow(g, base, MARGIN_X, W - MARGIN_X);
    }
  }
}
