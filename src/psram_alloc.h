#pragma once
// ArduinoJson allocator that keeps documents in PSRAM to spare internal RAM
// for WiFi/TLS and the LED DMA buffers.
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
  void deallocate(void* p) override { heap_caps_free(p); }
  void* reallocate(void* p, size_t n) override {
    return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
};

static SpiRamAllocator spiRam;  // stateless, one per file is fine
