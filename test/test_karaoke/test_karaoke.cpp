// Host tests: pio test -e native
#include <string.h>
#include <unity.h>

#include "karaoke/karaoke.h"

static Karaoke k, t;

void setUp() {}
void tearDown() {}

void test_defaults() {
  karaokeDefaults(k);
  TEST_ASSERT_EQUAL_STRING("Karaoke", k.title);
  TEST_ASSERT_EQUAL(0, k.n);
  for (int i = 0; i < KARAOKE_ROWS; i++) TEST_ASSERT_NOT_EQUAL(0, k.colors[i]);
}

void test_json_roundtrip() {
  karaokeDefaults(k);
  strcpy(k.title, "Wer singt?");
  k.titleColor = 0x29c6ff;
  k.colors[2] = 0x123456;
  k.n = 2;
  strcpy(k.entries[0].name, "Jörg");
  strcpy(k.entries[0].song, "Atemlos");
  strcpy(k.entries[1].name, "Anna");

  JsonDocument doc;
  karaokeToJson(k, doc.to<JsonObject>());
  TEST_ASSERT_EQUAL_STRING("#29c6ff", doc["titleColor"]);
  TEST_ASSERT_EQUAL_STRING("#123456", doc["colors"][2]);
  TEST_ASSERT_EQUAL(2, doc["entries"].size());
  TEST_ASSERT_TRUE(karaokeFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL_STRING("Wer singt?", t.title);
  TEST_ASSERT_EQUAL_HEX32(0x29c6ff, t.titleColor);
  TEST_ASSERT_EQUAL_HEX32(k.colors[0], t.colors[0]);
  TEST_ASSERT_EQUAL_HEX32(0x123456, t.colors[2]);
  TEST_ASSERT_EQUAL(2, t.n);
  TEST_ASSERT_EQUAL_STRING("Jörg", t.entries[0].name);
  TEST_ASSERT_EQUAL_STRING("Atemlos", t.entries[0].song);
  TEST_ASSERT_EQUAL_STRING("Anna", t.entries[1].name);
  TEST_ASSERT_EQUAL_STRING("", t.entries[1].song);
}

void test_json_clamps_and_rejects() {
  TEST_ASSERT_FALSE(karaokeFromJson(JsonVariantConst(), t));

  // no name: left out; bad colours: defaults; an empty title stays empty
  JsonDocument doc;
  deserializeJson(doc, R"({"title":"","colors":["red",7],"entries":[{"name":"","song":"x"},{"song":"y"},
    {"name":"Ben","song":"Wonderwall Wonderwall Wonderwall Wonderwall Wonderwall"},42]})");
  TEST_ASSERT_TRUE(karaokeFromJson(doc.as<JsonVariantConst>(), t));
  karaokeDefaults(k);
  TEST_ASSERT_EQUAL_STRING("", t.title);
  TEST_ASSERT_EQUAL_HEX32(k.colors[0], t.colors[0]);
  TEST_ASSERT_EQUAL_HEX32(k.colors[1], t.colors[1]);
  TEST_ASSERT_EQUAL(1, t.n);
  TEST_ASSERT_EQUAL_STRING("Ben", t.entries[0].name);
  TEST_ASSERT_EQUAL(sizeof(t.entries[0].song) - 1, strlen(t.entries[0].song));  // cut

  // at most MAX_KARAOKE
  doc.clear();
  JsonArray a = doc["entries"].to<JsonArray>();
  for (int i = 0; i < MAX_KARAOKE + 5; i++) a.add<JsonObject>()["name"] = "X";
  TEST_ASSERT_TRUE(karaokeFromJson(doc.as<JsonVariantConst>(), t));
  TEST_ASSERT_EQUAL(MAX_KARAOKE, t.n);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults);
  RUN_TEST(test_json_roundtrip);
  RUN_TEST(test_json_clamps_and_rejects);
  return UNITY_END();
}
