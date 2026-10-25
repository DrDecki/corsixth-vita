#!/bin/sh
set -e
cmake -S . -B build-vita \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_MOVIES=OFF \
  -DWITH_UPDATE_CHECK=OFF \
  -DWITH_MIDI_DEVICE=OFF
cmake --build build-vita -j"$(nproc)"
