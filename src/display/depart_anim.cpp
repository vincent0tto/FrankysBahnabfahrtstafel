#include "display/depart_anim.h"

#include <string.h>

// ---- Sprites ---------------------------------------------------------------
// '#' = lit LED. Vehicles face right (they drive to the right), all sprites
// stand on their last row.
static const int VEH_H = 11;

// Two-car tram: rear car, articulation, front car with pantograph and a
// sloped windscreen.
const char* const TRAM_ROWS[TRAM_H] = {
    "............................................#####..............",
    ".............................................#.#...............",
    "..###########################.....########################.....",
    ".#############################.#.###########################...",
    "##...#....#..#..#....#....#..#.#.#....#....#..#..#....#....##..",
    "##...#....#..#..#....#....#..#.#.#....#....#..#..#....#.....##.",
    "##...#....#..#..#....#....#..#.#.#....#....#..#..#....#......##",
    "###########..#..##############.#.###########..#..##############",
    "##############################.#.##############################",
    "..###.###............###.###.......###.###............###.###..",
    "...#...#..............#...#.........#...#..............#...#...",
};

// Boxy European city bus: flat roof with AC unit, two doors, flat front.
static const char* const BUS[VEH_H] = {
    "...####################.............",
    "####################################",
    "#....#....#..#....#....#..#...#....#",
    "#....#....#..#....#....#..#...#....#",
    "#....#....#..#....#....#..#...#....#",
    "###########..###########..#####....#",
    "###########..###########..##########",
    "###########..###########..##########",
    "####.....##..###########..#.....####",
    ".....#.#....................#.#.....",
    "......#......................#......",
};

static void drawSprite(Adafruit_GFX& g, uint16_t color, const char* const* rows, int h, int x,
                       int bottom, int top) {
  for (int r = 0; r < h; r++) {
    int y = bottom - (h - 1) + r;
    if (y < top) continue;
    for (int c = 0; rows[r][c]; c++)
      if (rows[r][c] == '#') g.drawPixel(x + c, y, color);
  }
}

// ---- Timeline (ms) ---------------------------------------------------------
// Vehicle: rolls in slowly from beyond the left edge and keeps accelerating
// until it has left the display (the tram, 63 px, just makes it in
// DEPART_ANIM_MS: 63 + 192 px).
static const int VEH_V0 = 12;     // px/s when its front appears
static const int VEH_ACCEL = 20;  // px/s^2

// Left edge of a `vehW` px wide vehicle at `t` ms.
static int vehicleX(uint32_t t, int vehW) {
  int64_t dt = t;
  return -vehW + (int)((VEH_V0 * dt * 1000 + (int64_t)VEH_ACCEL * dt * dt / 2) / 1000000);
}

void drawDepartAnim(Adafruit_GFX& g, uint16_t color, bool tram, uint32_t t, int top, int bottom,
                    int width) {
  const char* const* veh = tram ? TRAM_ROWS : BUS;
  int vehW = (int)strlen(veh[0]);
  int vx = vehicleX(t, vehW);

  int clearTo = vx + vehW + 1 < width ? vx + vehW + 1 : width;  // 1px gap to the text
  if (clearTo > 0) g.fillRect(0, top, clearTo, bottom - top + 1, 0);
  if (vx < width) drawSprite(g, color, veh, VEH_H, vx, bottom - 1, top);  // 1px above the baseline
}
