#pragma once
// A JSON GET over HTTPS (Let's Encrypt, include/certs.h), streamed into an
// ArduinoJson document through a filter. For the VRN (src/departures/vrn_client.cpp)
// and OpenLigaDB (src/match/match_client.cpp). Network task only.
#include <ArduinoJson.h>

enum class FetchResult { Ok, NetworkError, HttpError, ParseError };

const char* fetchResultName(FetchResult r);

// `tag` prefixes the log lines ("vrn"). `doc` only holds the fields in
// `filter`.
FetchResult httpsGetJson(const char* url, JsonDocument& filter, JsonDocument& doc, const char* tag);
