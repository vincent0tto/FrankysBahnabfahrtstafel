#include "core/log_ring.h"

#include <string.h>

void LogRing::append(const char* s, size_t n) {
  if (!cap_) return;
  if (n > cap_) {  // only the end fits
    s += n - cap_;
    total_ += n - cap_;
    n = cap_;
  }
  size_t at = total_ % cap_;
  size_t first = n < cap_ - at ? n : cap_ - at;
  memcpy(buf_ + at, s, first);
  memcpy(buf_, s + first, n - first);
  total_ += n;
}

size_t LogRing::read(uint32_t since, char* out, size_t outCap, uint32_t& start) const {
  // one byte less than stored, so the byte before `oldest` tells whether
  // it starts a line
  uint32_t oldest = total_ >= cap_ ? total_ - cap_ + 1 : 0;
  if (since < oldest || since > total_) {
    since = oldest;
    // the oldest line is cut off at the front, skip to the next one
    if (oldest > 0 && buf_[(oldest - 1) % cap_] != '\n') {
      while (since < total_ && buf_[since % cap_] != '\n') since++;
      if (since < total_) since++;
    }
  }
  start = since;
  size_t n = total_ - since;
  if (n > outCap) n = outCap;
  for (size_t i = 0; i < n; i++) out[i] = buf_[(since + i) % cap_];
  return n;
}
