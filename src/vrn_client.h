#pragma once
#include "departure.h"
#include "https_json.h"

// Fetches departures for STOP_ID (primary host, then fallback host) and fills
// `out` including fetchedAt. `out` is only modified on success.
FetchResult fetchBoard(Board& out);
