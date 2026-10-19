#pragma once
// Pixel icons for the promo texts. The built-in ones are drawn as strings
// ('#' = LED on), the ones made on the web page as bit rows.
#include <stdint.h>

#define ICON_W 12  // size of the icons made on the web page
#define ICON_H 9

struct PresetIcon {
  const char* tag;   // "[gc]", used in the settings
  const char* name;  // shown on the web page
  const char* const* rows;
  int w, h;
  int drop;  // px the icon reaches below the text baseline
};

extern const PresetIcon PRESET_ICONS[];
extern const int N_PRESET_ICONS;

const PresetIcon* findPresetIcon(const char* tag);
