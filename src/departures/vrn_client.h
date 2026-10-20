#pragma once
#include "core/https_json.h"
#include "departures/departure.h"

// Fetches departures for STOP_ID (primary host, then fallback host) and fills
// `out` including fetchedAt. `out` is only modified on success.
FetchResult fetchBoard(Board& out);
