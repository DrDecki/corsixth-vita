include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "CorsixTH")
set(VITA_TITLEID "CTHV00001")
set(VITA_VERSION "01.00")

set(VITA_SDK_INCLUDE "$ENV{VITASDK}/arm-vita-eabi/include")

target_compile_definitions(CorsixTH_lib PUBLIC _GNU_SOURCE)
target_compile_definitions(CorsixTH PRIVATE _GNU_SOURCE)

target_include_directories(CorsixTH_lib PUBLIC
  ${VITA_SDK_INCLUDE}/SDL2
  ${VITA_SDK_INCLUDE}/freetype2
)

target_link_libraries(CorsixTH_lib PUBLIC
  lfs
  lpeg
  lua
  SDL2_mixer
  modplug
  xmp
  opusfile
  opus
  FLAC
  vorbisfile
  vorbis
  ogg
  mpg123
  freetype
  bz2
  png
  z
  SDL2
  SceDisplay_stub
  SceCtrl_stub
  SceAudio_stub
  SceAudioIn_stub
  SceSysmodule_stub
  SceGxm_stub
  SceCommonDialog_stub
  SceAppMgr_stub
  SceTouch_stub
  SceHid_stub
  SceMotion_stub
  ScePower_stub
  SceProcessmgr_stub
  pthread
  m
)

target_link_options(CorsixTH PRIVATE -Wl,-q)

vita_create_self(eboot.bin CorsixTH)

set(VITA_DATA_DIR ${CMAKE_SOURCE_DIR}/CorsixTH)

vita_create_vpk(corsixth.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
  FILE ${CMAKE_SOURCE_DIR}/vita/sce_sys sce_sys
  FILE ${VITA_DATA_DIR}/CorsixTH.lua CorsixTH.lua
  FILE ${CMAKE_BINARY_DIR}/vita_lua Lua
  FILE ${CMAKE_BINARY_DIR}/lua.pak lua.pak
  FILE ${VITA_DATA_DIR}/Levels Levels
  FILE ${VITA_DATA_DIR}/Campaigns Campaigns
  FILE ${VITA_DATA_DIR}/Bitmap Bitmap
  FILE ${CMAKE_SOURCE_DIR}/LICENSE.txt LICENSE.txt
)
