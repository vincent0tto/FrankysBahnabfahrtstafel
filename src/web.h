#pragma once
// Settings web page on port 80 (http://abfahrtstafel.local): promos, icons,
// brightness, off time. No login: anyone in the WiFi can change them.
#include <time.h>

// Call once WiFi is up (again), then webLoop() often, both from the network task.
void webBegin();
void webLoop();

// For the status line: time of the last good API fetch.
void webNoteFetch(time_t fetchedAt);
