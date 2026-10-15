// Host tests: pio test -e native
#include <string.h>
#include <unity.h>

#include <string>

#include "log_ring.h"

void setUp() {}
void tearDown() {}

static std::string readFrom(const LogRing& r, uint32_t since, uint32_t& start, size_t cap = 64) {
  char out[64];
  size_t n = r.read(since, out, cap, start);
  return std::string(out, n);
}

static void add(LogRing& r, const char* s) { r.append(s, strlen(s)); }

void test_empty() {
  char buf[16];
  LogRing r(buf, sizeof buf);
  uint32_t start = 99;
  TEST_ASSERT_EQUAL_STRING("", readFrom(r, 0, start).c_str());
  TEST_ASSERT_EQUAL(0, start);
}

void test_read_since() {
  char buf[16];
  LogRing r(buf, sizeof buf);
  add(r, "ab\n");
  add(r, "cd\n");
  uint32_t start;
  TEST_ASSERT_EQUAL_STRING("ab\ncd\n", readFrom(r, 0, start).c_str());
  TEST_ASSERT_EQUAL(0, start);
  TEST_ASSERT_EQUAL_STRING("cd\n", readFrom(r, 3, start).c_str());
  TEST_ASSERT_EQUAL(3, start);
  TEST_ASSERT_EQUAL_STRING("", readFrom(r, 6, start).c_str());
  TEST_ASSERT_EQUAL(6, start);
}

void test_wrap_drops_partial_line() {
  char buf[9];  // holds 8
  LogRing r(buf, sizeof buf);
  add(r, "one\n");
  add(r, "two\n");
  add(r, "six\n");  // overwrites "one\n"
  TEST_ASSERT_EQUAL(12, r.total());
  uint32_t start;
  TEST_ASSERT_EQUAL_STRING("two\nsix\n", readFrom(r, 0, start).c_str());
  TEST_ASSERT_EQUAL(4, start);
  add(r, "x\n");  // now "o\nsix\nx\n": the cut line "o" is skipped
  TEST_ASSERT_EQUAL_STRING("six\nx\n", readFrom(r, 0, start).c_str());
  TEST_ASSERT_EQUAL(8, start);
  TEST_ASSERT_EQUAL_STRING("x\n", readFrom(r, 12, start).c_str());
}

void test_since_ahead_restarts() {
  // e.g. the page still has a position from before a reboot
  char buf[16];
  LogRing r(buf, sizeof buf);
  add(r, "new\n");
  uint32_t start;
  TEST_ASSERT_EQUAL_STRING("new\n", readFrom(r, 500, start).c_str());
  TEST_ASSERT_EQUAL(0, start);
}

void test_longer_than_buffer() {
  char buf[5];  // holds 4
  LogRing r(buf, sizeof buf);
  add(r, "abcdef");
  TEST_ASSERT_EQUAL(6, r.total());
  uint32_t start;
  TEST_ASSERT_EQUAL_STRING("cdef", readFrom(r, 2, start).c_str());
  // from an overwritten position: no newline in the buffer, the cut line is skipped
  TEST_ASSERT_EQUAL_STRING("", readFrom(r, 0, start).c_str());
  TEST_ASSERT_EQUAL(6, start);
}

void test_out_limit() {
  char buf[16];
  LogRing r(buf, sizeof buf);
  add(r, "abcdef\n");
  uint32_t start;
  TEST_ASSERT_EQUAL_STRING("abc", readFrom(r, 0, start, 3).c_str());
  TEST_ASSERT_EQUAL_STRING("def\n", readFrom(r, 3, start).c_str());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_empty);
  RUN_TEST(test_read_since);
  RUN_TEST(test_wrap_drops_partial_line);
  RUN_TEST(test_since_ahead_restarts);
  RUN_TEST(test_longer_than_buffer);
  RUN_TEST(test_out_limit);
  return UNITY_END();
}
