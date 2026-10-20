#include "icons.h"

#include <string.h>

static const char* const IG_ICON[] = {  // Instagram
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
// The Instagram logo is 2px taller, it sits 1px lower so it looks centred
// next to the letters; the others stand on the baseline like digits.
const PresetIcon PRESET_ICONS[] = {
    {"[ig]", "Instagram", IG_ICON, 11, 11, 1},
    {"[gc]", "Grilled Cheese", GC_ICON, 12, 9, 0},
    {"[fw]", "Flat White", FW_ICON, 12, 9, 0},
    {"[ku]", "Kuchen", KU_ICON, 12, 9, 0},
    {"[bi]", "Bier", BI_ICON, 12, 9, 0},
};
const int N_PRESET_ICONS = sizeof(PRESET_ICONS) / sizeof(PRESET_ICONS[0]);

const PresetIcon* findPresetIcon(const char* tag) {
  for (const PresetIcon& ic : PRESET_ICONS)
    if (strcmp(tag, ic.tag) == 0) return &ic;
  return nullptr;
}
