#pragma once
// Draws the event scenes (src/event.h) onto any Adafruit_GFX surface, like
// render.h for the departures: the same code runs on the LED panels, for the
// web page's preview (/api/event/preview) and in the host preview.
// No state of its own, so it can draw from both tasks at once.
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "event.h"

// `g` must be cleared beforehand. `ms`: time since the scenes started, picks
// the scene and scrolls texts too long for the board.
void renderEvent(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Event& e, uint32_t ms);

// One scene, `ms` after it came up.
void renderScene(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u8, const Scene& sc, uint32_t ms);

// Draws pixel image `id` as it is `ms` after its scene came up (on the board:
// src/image_store.cpp). Without one, scenes leave their images out.
typedef void (*ImageDrawer)(Adafruit_GFX& g, int id, uint32_t ms);
void setImageDrawer(ImageDrawer d);

// Draws a whole-board image (192 x 64 RGB565, row by row); 0 is transparent.
void drawPixels(Adafruit_GFX& g, const uint16_t* px);
