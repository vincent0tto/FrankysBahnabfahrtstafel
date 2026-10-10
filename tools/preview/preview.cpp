// Host preview of the LED layout: renders a few scenarios with the real
// render code into a PPM image (LED dots, 6x scale). Build/run: tools/preview/run.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "config.h"
#include "parse.h"
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

static Frame render(const Board& b, DisplayStatus st, time_t now, uint32_t ms) {
  GFXcanvas16 canvas(W, H);
  canvas.fillScreen(0);
  U8G2_FOR_ADAFRUIT_GFX u8;
  u8.begin(canvas);
  uint16_t amber = ((COLOR_R & 0xF8) << 8) | ((COLOR_G & 0xFC) << 3) | (COLOR_B >> 3);
  renderFrame(canvas, u8, amber, b, st, now, ms);
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

  std::vector<Frame> frames;
  Board normal = fixtureBoard(now);
  frames.push_back(render(normal, ok, now, 0));
  frames.push_back(render(normal, ok, now, PLATFORM_EVERY_S * 1000 - 1));  // platforms instead of times

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
  static const char* const promos[] = PROMO_TEXTS;
  Board evening = normal;
  for (int i = 0; i < evening.n; i++) evening.deps[i].when += 6 * 3600;
  evening.fetchedAt += 6 * 3600;
  for (int i = 0; i < (int)(sizeof(promos) / sizeof(promos[0])); i++) {
    setPromoIndex(i);
    render(evening, ok, now + 6 * 3600, 2000000 + i * 1000000);  // ends the previous one
    frames.push_back(render(evening, ok, now + 6 * 3600, 2000000 + i * 1000000 + 500000));
  }
  setPromoIndex(-1);

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
  return 0;
}
