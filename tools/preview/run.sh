#!/bin/sh
# Renders tools/preview/preview.png from the real render code (run from project root).
# Needs: pio pkg install -e esp32s3 (for the libraries). Uses the Xcode clang by
# default; override CXX/CC/LDFLAGS if needed.
set -e
L=.pio/libdeps/esp32s3
OUT=${OUT:-tools/preview/build}
mkdir -p "$OUT"
CXX=${CXX:-clang++}
CC=${CC:-clang}
INC="-DARDUINO=100 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 -Itools/preview/shim -Iinclude -Isrc -I$L/ArduinoJson/src -I$L/Adafruit\ GFX\ Library -I$L/U8g2_for_Adafruit_GFX/src"
eval $CC $CFLAGS -c $INC $L/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c -o $OUT/u8g2_fonts.o
eval $CXX -std=c++17 $CXXFLAGS $INC $LDFLAGS -Wno-macro-redefined \
  tools/preview/preview.cpp src/render.cpp src/depart_anim.cpp src/parse.cpp \
  \"$L/Adafruit GFX Library/Adafruit_GFX.cpp\" $L/U8g2_for_Adafruit_GFX/src/U8g2_for_Adafruit_GFX.cpp \
  $OUT/u8g2_fonts.o -o $OUT/preview
$OUT/preview $OUT/preview.ppm $OUT/preview_anim.ppm
sips -s format png $OUT/preview.ppm --out tools/preview/preview.png >/dev/null
sips -s format png $OUT/preview_anim.ppm --out tools/preview/preview_anim.png >/dev/null
echo "tools/preview/preview.png tools/preview/preview_anim.png"
