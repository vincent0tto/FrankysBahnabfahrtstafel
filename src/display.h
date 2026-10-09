#pragma once
#include <stdint.h>
#include <time.h>

#include "departure.h"
#include "render.h"

bool displayBegin();

// Draws one complete frame into the back buffer and flips it.
// Call ~30x per second (scrolling texts); everything else is derived from
// `now`, so cached departures keep counting down while the API is down.
void displayRender(const Board& b, const DisplayStatus& st, time_t now, uint32_t ms);
