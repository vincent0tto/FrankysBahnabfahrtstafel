#pragma once
#include "departure.h"

enum class FetchResult { Ok, NetworkError, HttpError, ParseError };

const char* fetchResultName(FetchResult r);

// Fetches departures for STOP_ID (primary host, then fallback host) and fills
// `out` including fetchedAt. `out` is only modified on success.
FetchResult fetchBoard(Board& out);
