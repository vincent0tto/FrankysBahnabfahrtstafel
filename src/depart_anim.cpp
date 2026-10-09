#include "depart_anim.h"

#include <string.h>

// ---- Sprites ---------------------------------------------------------------
// '#' = lit LED. Vehicles face right (they drive to the right), all sprites
// stand on their last row.
static const int VEH_H = 11;

// Two-car tram: rear car, articulation, front car with pantograph and a
// sloped windscreen.
static const char* const TRAM[VEH_H] = {
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

static const char* const BUS[VEH_H] = {
    "........#######.................",
    ".##############################.",
    "################################",
    "#...#...#...#..#...#...#..#....#",
    "#...#...#...#..#...#...#..#....#",
    "############..##########..######",
    "############..##########..######",
    "################################",
    "####.....###############.....###",
    ".....#.#.................#.#....",
    "......#...................#.....",
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
// Vehicle: stands at the left edge for a moment (doors closing), then pulls
// away smoothly and accelerates out of the display.
static const int VEH_X0 = 2;
static const uint32_t VEH_HOLD = 600;
static const int VEH_ACCEL = 26;  // px/s^2

static int vehicleX(uint32_t t) {
  if (t < VEH_HOLD) return VEH_X0;
  int64_t dt = t - VEH_HOLD;
  return VEH_X0 + (int)((int64_t)VEH_ACCEL * dt * dt / 2000000);
}

void drawDepartAnim(Adafruit_GFX& g, uint16_t color, bool tram, uint32_t t, int top, int bottom,
                    int width) {
  const char* const* veh = tram ? TRAM : BUS;
  int vehW = (int)strlen(veh[0]);
  int vx = vehicleX(t);

  int clearTo = vx + vehW + 1 < width ? vx + vehW + 1 : width;  // 1px gap to the text
  g.fillRect(0, top, clearTo, bottom - top + 1, 0);
  if (vx < width) drawSprite(g, color, veh, VEH_H, vx, bottom, top);
}
