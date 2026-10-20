// Host preview of the LED layout: renders a few scenarios with the real
// render code into PPM images (LED dots, 6x scale): the departures, the
// departure animation, the event mode and the tram race.
// Build/run: tools/preview/run.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "config.h"
#include "event_render.h"
#include "parse.h"
#include "race_render.h"
#include "render.h"

static const int W = PANEL_RES_X * PANEL_CHAIN, H = PANEL_RES_Y, S = 6, GAP = 14;

static time_t iso(const char* s) {
  time_t t = 0;
  parseIsoUtc(s, t);
  return t;
}

static Board fixtureBoard(time_t now) {
  std::ifstream f("test/fixtures/schriesheim.json");
  std::stringstream ss;
  ss << f.rdbuf();
  JsonDocument filter, doc;
  buildFilter(filter);
  deserializeJson(doc, ss.str(), DeserializationOption::Filter(filter));
  ParseOptions opt;
  opt.abbreviations = DEST_ABBREVIATIONS;
  opt.platformLabels = PLATFORM_LABELS;
  Board b;
  parseDepartures(doc.as<JsonVariantConst>(), now, opt, b);
  b.fetchedAt = now;
  return b;
}

struct Frame { std::vector<uint16_t> px; };

static Settings settings;  // config.h defaults

static Frame render(const Board& b, DisplayStatus st, time_t now, uint32_t ms) {
  GFXcanvas16 canvas(W, H);
  canvas.fillScreen(0);
  U8G2_FOR_ADAFRUIT_GFX u8;
  u8.begin(canvas);
  uint16_t amber = ((COLOR_R & 0xF8) << 8) | ((COLOR_G & 0xFC) << 3) | (COLOR_B >> 3);
  renderFrame(canvas, u8, amber, b, settings, st, now, ms);
  Frame f;
  f.px.assign(canvas.getBuffer(), canvas.getBuffer() + W * H);
  return f;
}

// A pixel image for the event preview: a cocktail glass at double size on
// the left, as one would draw it on the event page.
static uint16_t cocktail[W * H];
static void makeCocktail() {
  auto set = [](int x, int y, uint32_t rgb) {
    for (int dy = 0; dy < 2; dy++)
      for (int dx = 0; dx < 2; dx++) cocktail[(8 + 2 * y + dy) * W + 10 + 2 * x + dx] = rgb565(rgb);
  };
  for (int y = 3; y <= 12; y++)
    for (int x = 2 + (y - 3); x <= 21 - (y - 3); x++)
      set(x, y, y == 3 || x == 2 + (y - 3) || x == 21 - (y - 3) ? 0xCFEFFF : y <= 6 ? 0xFFB000 : 0xFF6A00);
  for (int y = 13; y <= 17; y++) set(11, y, 0xCFEFFF), set(12, y, 0xCFEFFF);
  for (int x = 7; x <= 16; x++) set(x, 18, 0xCFEFFF);
  for (int i = 0; i < 6; i++) set(18 - i, i, 0xFF5FAE);
  set(5, 1, 0xFF2A3A), set(6, 1, 0xFF2A3A), set(5, 2, 0xFF2A3A), set(6, 2, 0xFF2A3A);
}

static Frame renderEventFrame(const Event& e, uint32_t ms) {
  GFXcanvas16 canvas(W, H);
  canvas.fillScreen(0);
  U8G2_FOR_ADAFRUIT_GFX u8;
  u8.begin(canvas);
  renderEvent(canvas, u8, e, ms);
  Frame f;
  f.px.assign(canvas.getBuffer(), canvas.getBuffer() + W * H);
  return f;
}

static void writePpm(const char* path, const std::vector<Frame>& frames) {
  int iw = W * S, ih = (int)frames.size() * (H * S + GAP) - GAP;
  std::vector<uint8_t> img((size_t)iw * ih * 3, 40);
  for (size_t fi = 0; fi < frames.size(); fi++) {
    int oy = (int)fi * (H * S + GAP);
    for (int y = 0; y < H * S; y++)
      for (int x = 0; x < iw; x++) {
        uint16_t c = frames[fi].px[(y / S) * W + x / S];
        int dx = x % S - S / 2, dy = y % S - S / 2;
        bool dot = dx * dx + dy * dy <= (S / 2 - 1) * (S / 2 - 1) + 1;
        uint8_t r = 12, g = 12, bl = 12;  // dark LED
        if (c && dot) { r = (c >> 11) << 3; g = ((c >> 5) & 0x3F) << 2; bl = (c & 0x1F) << 3; }
        else if (!dot) r = g = bl = 0;
        uint8_t* p = &img[((size_t)(oy + y) * iw + x) * 3];
        p[0] = r; p[1] = g; p[2] = bl;
      }
  }
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6\n%d %d\n255\n", iw, ih);
  fwrite(img.data(), 1, img.size(), f);
  fclose(f);
  printf("wrote %s (%zu frames)\n", path, frames.size());
}

// The departure animation: row 0 shows "sofort", then its minute is over.
// `both`: row 1 leaves in the same minute too -> no animation.
static void departAnim(std::vector<Frame>& frames, Board b, uint8_t cls, time_t now,
                       std::initializer_list<uint32_t> times, bool both = false) {
  DisplayStatus ok;
  ok.wifi = ok.timeValid = true;
  b.deps[0].when = now + 30;
  if (both) b.deps[1].when = now + 40;
  b.deps[0].cls = cls;
  b.fetchedAt = now;
  setDepartAnimChance(1);
  render(b, ok, now, 0);  // remembers the shown rows
  const uint32_t t0 = 100000;
  for (uint32_t t : times) frames.push_back(render(b, ok, now + 60 + t / 1000, t0 + t));
  render(b, ok, now + 600, t0 + 600000);  // let it finish
  setDepartAnimChance(0);
}

int main(int argc, char** argv) {
  setenv("TZ", TZ_INFO, 1);
  tzset();
  const char* out = argc > 1 ? argv[1] : "preview.ppm";
  time_t now = iso("2026-10-08T10:35:00Z");
  DisplayStatus ok;
  ok.wifi = ok.timeValid = true;
  setDepartAnimChance(0);
  settingsDefaults(settings);

  std::vector<Frame> frames;
  Board normal = fixtureBoard(now);
  frames.push_back(render(normal, ok, now, 0));
  frames.push_back(render(normal, ok, now, PLATFORM_EVERY_S * 1000 - 1));  // platforms instead of times
  Board mixed = normal;  // labels of different glyph widths must line up
  const char* labels[] = {"A", "1", "B", "2"};
  for (int i = 0; i < 4; i++) strcpy(mixed.deps[i].platformLabel, labels[i]);
  mixed.deps[1].cancelled = true;  // still shows its platform
  frames.push_back(render(mixed, ok, now, PLATFORM_EVERY_S * 1000 - 1));

  // long destination: start, mid-scroll, end
  Board longDest = normal;
  strcpy(longDest.deps[1].dest, "Heidelberg Bismarckplatz über Dossenheim");
  longDest.deps[2].cancelled = true;
  longDest.deps[3].when = iso("2026-10-08T11:40:00Z");
  for (uint32_t ms : {0u, 4000u, 9000u}) frames.push_back(render(longDest, ok, now, ms));

  Board ticker = normal;
  strcpy(ticker.ticker, "Schriesheim: Wegen Bauarbeiten fährt die Linie 628 eine Umleitung");
  frames.push_back(render(ticker, ok, now, 3000));

  Board empty;
  empty.fetchedAt = now;
  frames.push_back(render(empty, ok, now, 0));
  DisplayStatus offline = ok;
  offline.wifi = false;
  frames.push_back(render(normal, offline, now + 400, 0));

  // promo text in the 4th row (the timer is due long after the frames above)
  frames.push_back(render(normal, ok, now, 1000000));
  // each promo once; 18:35 local so the "@16" ones are open too
  Board evening = normal;
  for (int i = 0; i < evening.n; i++) evening.deps[i].when += 6 * 3600;
  evening.fetchedAt += 6 * 3600;
  // plus own ones from the web page: plain text with an own icon, and
  // departure-style with an own icon
  CustomIcon& heart = settings.icons[0];
  heart.used = true;
  const char* heartRows[ICON_H] = {"..##...##...", ".####.####..", "##########..", "##########..",
                                   ".########...", "..######....", "...####.....", "....##......", "............"};
  for (int r = 0; r < ICON_H; r++)
    for (int c = 0; c < ICON_W; c++)
      if (heartRows[r][c] == '#') heart.rows[r] |= 1 << (ICON_W - 1 - c);
  Promo own;
  strcpy(own.icon, "[c0]");
  strcpy(own.text, "Danke fürs Vorbeikommen");
  settings.promos[settings.nPromos++] = own;
  strcpy(own.text, "Waffeln");
  strcpy(own.right, "gleich");
  settings.promos[settings.nPromos++] = own;
  for (int i = 0; i < settings.nPromos; i++) {
    setPromoIndex(i);
    render(evening, ok, now + 6 * 3600, 2000000 + i * 1000000);  // ends the previous one
    frames.push_back(render(evening, ok, now + 6 * 3600, 2000000 + i * 1000000 + 500000));
  }
  setPromoIndex(-1);
  settingsDefaults(settings);

  DisplayStatus updating = ok;
  updating.updating = true;
  frames.push_back(render(normal, updating, now, 0));

  // long destination scrolling past the right edge: tall letters must be cut
  Board tall = normal;
  strcpy(tall.deps[0].line, "628");
  strcpy(tall.deps[0].dest, "Wilhelmsfeld Autohalle Ortsmitte");
  for (uint32_t ms : {2500u, 2900u}) frames.push_back(render(tall, ok, now, ms));

  writePpm(out, frames);

  std::vector<Frame> anim;
  departAnim(anim, normal, 4, now, {0, 1200, 2000, 2600, 3100, 3500, 3900, 4700});
  departAnim(anim, normal, 5, now + 3600, {1000, 2000, 3000});
  departAnim(anim, normal, 4, now, {1000}, true);
  writePpm(argc > 2 ? argv[2] : "preview_anim.ppm", anim);

  // event mode: the example scene, every text size and position, a coloured
  // background, a text too long for the board (start and scrolled), the
  // animations, the transitions and a pixel image
  std::vector<Frame> event;
  Event ev;
  eventDefaults(ev);
  event.push_back(renderEventFrame(ev, 0));
  Scene sizes;
  const char* sizeTexts[] = {"Klein oben links", "Normal Mitte", "Groß unten", "Riesig"};
  for (int i = 0; i < 3; i++) {
    EventLine& l = sizes.lines[sizes.nLines++];
    strcpy(l.text, sizeTexts[i]);
    l.size = (TextSize)i;
    l.v = (VPos)i;
    l.h = (HAlign)i;
    l.color = i == 0 ? 0xFFD400 : i == 1 ? 0x29C6FF : 0xFF3BD0;
  }
  ev.scenes[0] = sizes;
  event.push_back(renderEventFrame(ev, 0));
  Scene huge;
  huge.bg = 0x1A0B2E;
  EventLine& h = huge.lines[huge.nLines++];
  strcpy(h.text, "PARTY");
  h.size = TextSize::Huge;
  h.color = 0xFFB000;
  ev.scenes[0] = huge;
  event.push_back(renderEventFrame(ev, 0));
  strcpy(h.text, "Ölige Äpfel für Jürgen");
  ev.scenes[0] = huge;
  for (uint32_t ms : {0u, 6000u}) event.push_back(renderEventFrame(ev, ms));

  // animations: rainbow + wave, typewriter half-way, ticker, pulse dimmed
  Scene fx;
  const char* fxTexts[] = {"Regenbogen-Welle", "Schreibmaschine", "Laufschrift von rechts"};
  const Anim fxAnims[] = {Anim::Wave, Anim::Type, Anim::Scroll};
  for (int i = 0; i < 3; i++) {
    EventLine& l = fx.lines[fx.nLines++];
    strcpy(l.text, fxTexts[i]);
    l.v = (VPos)i;
    l.anim = fxAnims[i];
    l.rainbow = i == 0;
    l.color = 0x4BE04B;
  }
  ev.scenes[0] = fx;
  for (uint32_t ms : {800u, 3000u}) event.push_back(renderEventFrame(ev, ms));
  // transitions: slide and wipe half-way from the example scene into "PARTY"
  eventDefaults(ev);
  ev.nScenes = 2;
  strcpy(h.text, "PARTY");
  ev.scenes[1] = huge;
  for (Transition tr : {Transition::Slide, Transition::Wipe}) {
    ev.scenes[0].trans = tr;
    event.push_back(renderEventFrame(ev, ev.scenes[0].durS * 1000 + TRANSITION_MS / 2));
  }
  // a pixel image under text on a coloured background
  makeCocktail();
  setImageDrawer([](Adafruit_GFX& g, int, uint32_t) { drawPixels(g, cocktail); });
  Scene pic;
  pic.bg = 0x1A0B2E;
  pic.img = 0;
  const char* picTexts[] = {"HAPPY", "HOUR"};
  for (int i = 0; i < 2; i++) {
    EventLine& l = pic.lines[pic.nLines++];
    strcpy(l.text, picTexts[i]);
    l.size = TextSize::Large;
    l.v = i ? VPos::Bottom : VPos::Top;
    l.h = HAlign::Right;
    l.color = i ? 0x29C6FF : 0xFF3BD0;
  }
  eventDefaults(ev);
  ev.scenes[0] = pic;
  event.push_back(renderEventFrame(ev, 0));
  writePpm(argc > 3 ? argv[3] : "preview_event.ppm", event);

  // tram race: countdown, start, mid-race (one gliding), finish, winner
  // blinking, the lap of honour
  std::vector<Frame> raceFrames;
  static Race race;
  raceBegin(race, 12345);
  const uint32_t C = RACE_COUNTDOWN_MS, E = C + race.endMs;
  for (uint32_t t : {100u, C - 300, C + 1000, C + race.endMs / 2, C + race.endMs / 2 + 150, E, E + 300,
                     E + 2600, E + 5000, E + 8000}) {
    GFXcanvas16 canvas(W, H);
    canvas.fillScreen(0);
    U8G2_FOR_ADAFRUIT_GFX u8;
    u8.begin(canvas);
    drawRace(canvas, u8, ((COLOR_R & 0xF8) << 8) | ((COLOR_G & 0xFC) << 3) | (COLOR_B >> 3), race, t);
    raceFrames.push_back(Frame{std::vector<uint16_t>(canvas.getBuffer(), canvas.getBuffer() + W * H)});
  }
  printf("race: winner %d after %u ms, %d moves\n", race.winner + 1, (unsigned)race.endMs, race.nMoves);
  writePpm(argc > 4 ? argv[4] : "preview_race.ppm", raceFrames);
  return 0;
}
