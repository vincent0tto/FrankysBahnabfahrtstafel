#pragma once
// Departure animation: the departing tram/bus drives out of its row. Pure
// drawing, no state, so it renders the same on the panels and in the host
// preview.
#include <Adafruit_GFX.h>
#include <stdint.h>

// Total length of the animation.
static const uint32_t DEPART_ANIM_MS = 4600;

// Draws animation time `t` (0..DEPART_ANIM_MS) into the row slice
// [top, bottom] (inclusive). Everything left of the vehicle's front is cleared,
// text right of it stays visible. `tram` picks the vehicle sprite.
void drawDepartAnim(Adafruit_GFX& g, uint16_t color, bool tram, uint32_t t, int top, int bottom,
                    int width);

// Draws the tram (`tram`) or bus sprite with its left edge at `x`, its lowest
// row at `bottom`, nothing above `top`. Returns its width.
int drawVehicle(Adafruit_GFX& g, uint16_t color, bool tram, int x, int bottom, int top);
