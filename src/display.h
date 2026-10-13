#pragma once
#include <stdint.h>
#include <time.h>

#include "departure.h"
#include "render.h"
#include "settings.h"

bool displayBegin();

// Draws one complete frame into the back buffer and flips it, with the
// brightness from `s`. Call ~30x per second (scrolling texts); everything
// else is derived from `now`, so cached departures keep counting down while
// the API is down. Within the off time the panel stays dark and nothing is
// drawn; returns true then, so the caller can call less often.
bool displayRender(const Board& b, const Settings& s, const DisplayStatus& st, time_t now, uint32_t ms);
