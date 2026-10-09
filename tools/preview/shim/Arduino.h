// Minimal Arduino shim so Adafruit GFX + U8g2_for_Adafruit_GFX build on the host.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Print.h"
#include "pgmspace.h"
typedef bool boolean;
class __FlashStringHelper;
struct String {  // only what Adafruit_GFX::getTextBounds needs
  const char* s = "";
  unsigned int length() const { return strlen(s); }
  const char* c_str() const { return s; }
  char operator[](unsigned int i) const { return s[i]; }
};
#define radians(deg) ((deg) * M_PI / 180.0)
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
