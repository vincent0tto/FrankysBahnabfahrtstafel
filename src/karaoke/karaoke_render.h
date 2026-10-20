#pragma once
// Draws the karaoke list (src/karaoke/karaoke.h) like render.h the departures: a
// heading and the first KARAOKE_ROWS entries, name and song in columns, too
// long ones moving to and fro. For the panel, the web page's preview
// (/api/karaoke/preview) and the host preview.
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "karaoke/karaoke.h"

// `g` must be cleared beforehand; `ms` drives the scrolling.
void renderKaraoke(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Karaoke& k, uint32_t ms);
