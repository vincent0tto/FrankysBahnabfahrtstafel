#pragma once
// Ring buffer for the newest log text. Positions count every byte ever
// appended, so a reader asks for "everything after position N" and notices
// when part of it has already been overwritten. Not thread-safe (src/log.cpp
// locks around it).
#include <stddef.h>
#include <stdint.h>

class LogRing {
 public:
  LogRing() = default;
  LogRing(char* buf, size_t cap) : buf_(buf), cap_(cap) {}

  void append(const char* s, size_t n);

  // Copies up to `outCap` bytes from position `since` on into `out` and sets
  // `start` to where they begin. If `since` is no longer (or not yet) in the
  // buffer, reading starts at the oldest complete line instead. Holds the
  // newest cap - 1 bytes.
  size_t read(uint32_t since, char* out, size_t outCap, uint32_t& start) const;

  uint32_t total() const { return total_; }

 private:
  char* buf_ = nullptr;
  size_t cap_ = 0;
  uint32_t total_ = 0;
};
