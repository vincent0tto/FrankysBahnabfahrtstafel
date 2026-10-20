#pragma once
// Pixel images for the event scenes: whole-board pictures (192 x 64 RGB565,
// 0 = transparent), up to MAX_IMAGE_FRAMES frames that run as an animation.
// Each is a file "/img<id>.bin" on the event's file system (src/event/event_store.h)
// and kept in PSRAM while the board runs. Drawn and edited on the event page.
#include <Adafruit_GFX.h>
#include <stdint.h>

#include "event/event.h"

#define IMAGE_W            192
#define IMAGE_H             64
#define IMAGE_FRAME_BYTES  (IMAGE_W * IMAGE_H * 2)
#define IMAGE_NAME_LEN      24

struct ImageInfo {
  bool used = false;
  char name[IMAGE_NAME_LEN] = "";
  uint8_t frames = 0;
  uint16_t delayMs = 0;  // per frame
};

// Loads the stored images. Call once in setup(), after eventBegin(); also
// makes the event's scenes draw them.
void imageBegin();

// Network task only (it is the only writer):
ImageInfo imageInfo(int id);
const uint16_t* imagePixels(int id);  // all its frames, nullptr if none

// Stores `frames` frames from `px` (PSRAM, taken over and freed in any case)
// as image `id`, replacing one that is there. false with `err` if it can't
// (not enough space, file system missing).
bool imageStore(int id, const char* name, uint8_t frames, uint16_t delayMs, uint16_t* px, const char*& err);
bool imageDelete(int id);

// Bytes free on the file system, total.
size_t imageFreeBytes();
size_t imageTotalBytes();

// Any task: draws image `id` (nothing if there is none) as it is `ms` after
// its scene came up.
void imageDraw(Adafruit_GFX& g, int id, uint32_t ms);
