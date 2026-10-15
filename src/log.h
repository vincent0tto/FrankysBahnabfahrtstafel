#pragma once
// Log output: goes to the serial port as before and, with a time stamp per
// line, into a ring buffer in PSRAM that the settings page shows (/api/log).
// Use Log.printf()/Log.println() instead of Serial.
#include <Print.h>
#include <stdint.h>

class LogOut : public Print {
 public:
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* buf, size_t n) override;
};

extern LogOut Log;

// Call once in setup() right after Serial.begin().
void logBegin();

// Copies the log text from position `since` on (see src/log_ring.h) into
// `out`. `start` is where it begins; a jump means lines were lost.
size_t logRead(uint32_t since, char* out, size_t outCap, uint32_t& start);

// Random per start, so the page notices a reboot.
uint32_t logBootId();
