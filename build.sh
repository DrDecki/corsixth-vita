#!/bin/sh
set -e

LUA_SRC="$HOME/vita-luadeps/lua"
HOST_LUA=build-vita/host-lua

mkdir -p build-vita
if [ ! -x "$HOST_LUA" ]; then
  gcc -O2 -w -o "$HOST_LUA" $(ls "$LUA_SRC"/*.c | grep -v -e ltests.c -e onelua.c -e luac.c) -lm
fi

rm -rf build-vita/vita_lua
cp -r CorsixTH/Lua build-vita/vita_lua
for f in CorsixTH/Lua/languages/*.lua; do
  name=$(basename "$f")
  "$HOST_LUA" vita/compile_lua.lua "$f" "build-vita/vita_lua/languages/$name" "app0:/Lua/languages/$name"
done

cmake -S . -B build-vita \
  -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_MOVIES=OFF \
  -DWITH_UPDATE_CHECK=OFF \
  -DWITH_MIDI_DEVICE=OFF
rm -f build-vita/CorsixTH/corsixth.vpk
cmake --build build-vita -j"$(nproc)"
