/*
Copyright (c) 2009 Peter "Corsix" Cawley

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "../Src/main.h"

#include "config.h"

#include <SDL.h>
#include <SDL_mixer.h>

#include <cstdio>
#include <memory>

#include "../Src/bootstrap.h"
#include "../Src/lua.hpp"
#include "../Src/sdl_core.h"
#include "../Src/th_lua.h"
#ifdef WITH_UPDATE_CHECK
#include <curl/curl.h>
#endif
#ifdef WITH_TRACY
#include <tracy/Tracy.hpp>
#include <tracy/TracyLua.hpp>
#endif
#ifdef __vita__
#include <psp2/io/fcntl.h>
#include <psp2/power.h>
#include <sys/stat.h>

#include <cstdlib>
#include <cstring>
#include <string>

static void vita_write(const char* path, const char* data, size_t len) {
  SceUID fd =
      sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND, 0777);
  if (fd >= 0) {
    sceIoWrite(fd, data, len);
    sceIoClose(fd);
  }
}

static void vita_trace(const char* msg) {
  vita_write("ux0:/data/corsixth_trace.txt", msg, std::strlen(msg));
}

static void vita_log(const char* data, size_t len) {
  vita_write("ux0:/data/corsixth/log.txt", data, len);
}

__attribute__((constructor(101))) static void vita_trace_ctor() {
  vita_trace("ctor\n");
}

int _newlib_heap_size_user = 192 * 1024 * 1024;
unsigned int sceUserMainThreadStackSize = 2 * 1024 * 1024;

static void vita_init() {
  vita_trace("main\n");
  scePowerSetArmClockFrequency(444);
  scePowerSetBusClockFrequency(222);
  scePowerSetGpuClockFrequency(222);
  scePowerSetGpuXbarClockFrequency(166);
  vita_trace("clocks\n");

  mkdir("ux0:/data/corsixth", 0777);
  vita_trace("mkdir\n");
  SceUID log_fd = sceIoOpen("ux0:/data/corsixth/log.txt",
                            SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
  if (log_fd >= 0) sceIoClose(log_fd);
  vita_trace("log\n");

  setenv("HOME", "ux0:/data/corsixth", 1);
  setenv("USER", "PLAYER", 1);
  setenv("TIMIDITY_CFG", "ux0:/data/corsixth/timidity/timidity.cfg", 1);
  SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1);
  SDL_setenv("VITA_DISABLE_TOUCH_FRONT", "1", 1);
  vita_trace("env\n");
}

extern unsigned long long vita_persist_us[4];
const std::string* vita_pack_find(const char* path);

static int vita_pack_read(lua_State* L) {
  const std::string* data = vita_pack_find(luaL_checkstring(L, 1));
  if (!data) return 0;
  lua_pushlstring(L, data->data(), data->size());
  return 1;
}

static const char* vita_io_open_wrapper = R"lua(
local real_open = io.open
local pack_read = vita_pack_read
io.open = function(path, mode)
  if type(path) == "string" and (mode == nil or mode == "r" or mode == "rb") then
    local data = pack_read(path)
    if data then
      local pos = 1
      local f = {}
      function f:read(fmt)
        fmt = fmt or "l"
        if type(fmt) == "number" then
          if pos > #data then return nil end
          local s = data:sub(pos, pos + fmt - 1)
          pos = pos + #s
          return s
        end
        local c = fmt:gsub("^%*", ""):sub(1, 1)
        if c == "a" then
          local s = data:sub(pos)
          pos = #data + 1
          return s
        elseif c == "l" or c == "L" then
          if pos > #data then return nil end
          local e = data:find("\n", pos, true)
          local line
          if e then
            line = data:sub(pos, c == "L" and e or e - 1)
            pos = e + 1
          else
            line = data:sub(pos)
            pos = #data + 1
          end
          return line
        end
        return nil
      end
      function f:lines(fmt)
        return function() return f:read(fmt or "l") end
      end
      function f:seek(whence, offset)
        whence = whence or "cur"
        offset = offset or 0
        if whence == "set" then
          pos = offset + 1
        elseif whence == "end" then
          pos = #data + offset + 1
        else
          pos = pos + offset
        end
        return pos - 1
      end
      function f:close() return true end
      return f
    end
  end
  return real_open(path, mode)
end
)lua";

static int vita_persist_stats(lua_State* L) {
  lua_pushfstring(L,
                  "[time] persist open %d ms, read %d ms, parse %d ms, scan %d ms",
                  static_cast<int>(vita_persist_us[3] / 1000),
                  static_cast<int>((vita_persist_us[0] - vita_persist_us[3]) / 1000),
                  static_cast<int>(vita_persist_us[1] / 1000),
                  static_cast<int>(vita_persist_us[2] / 1000));
  return 1;
}

static int vita_lua_print(lua_State* L) {
  int n = lua_gettop(L);
  for (int i = 1; i <= n; ++i) {
    size_t len;
    const char* str = luaL_tolstring(L, i, &len);
    if (i > 1) vita_log("\t", 1);
    vita_log(str, len);
    lua_pop(L, 1);
  }
  vita_log("\n", 1);
  return 0;
}
#endif

// Template magic for checking type equality
template <typename T1, typename T2>
struct types_equal {
  enum {
    result = -1,
  };
};

template <typename T1>
struct types_equal<T1, T1> {
  enum {
    result = 1,
  };
};

#ifdef WITH_TRACY

/// Replacement Lua Hook for Tracy Lua instrumentation
/**
 * As of this implementation the lua hook included in Tracy does not handle
 * errors caught by pcall, so we implement our own hook that does. See:
 * https://github.com/wolfpld/tracy/issues/1320
 */
void l_tracy_hook(lua_State* L, lua_Debug* ar) {
  using namespace tracy;
  static int depth = 0;
  if (ar->event == LUA_HOOKCALL) {
    lua_getinfo(L, "Snl", ar);
    depth++;

    char src[256];
    detail::LuaShortenSrc(src, ar->short_src);

    const auto srcloc = Profiler::AllocSourceLocation(
        ar->currentline, src, ar->name ? ar->name : ar->short_src);
    TracyQueuePrepare(QueueType::ZoneBeginAllocSrcLoc);
    MemWrite(&item->zoneBegin.time, Profiler::GetTime());
    MemWrite(&item->zoneBegin.srcloc, srcloc);
    TracyQueueCommit(zoneBeginThread);
  } else if (ar->event == LUA_HOOKRET) {
    do {
      depth--;
      TracyQueuePrepare(QueueType::ZoneEnd);
      MemWrite(&item->zoneEnd.time, Profiler::GetTime());
      TracyQueueCommit(zoneEndThread);
    } while (lua_getstack(L, depth, ar) == 0);
  }
}

#endif

//! Program entry point
/*!
    Prepares a Lua state for, and catches errors from, lua_init(). By
    executing in Lua mode as soon as possible, errors can be nicely caught
    sooner, hence this function does as little as possible and leaves the rest
    for lua_init().
*/
int main(int argc, char** argv) {
  struct compile_time_lua_check {
    // Lua 5.1, not 5.0, is required
    int lua_5_point_1_required[LUA_VERSION_NUM >= 501 ? 1 : -1];

    // Lua numbers must be doubles so that the mantissa has at least
    // 32 bits (floats only have 24 bits)
    int number_is_double[types_equal<lua_Number, double>::result];
  };

#ifdef WITH_UPDATE_CHECK
  curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
#ifdef __vita__
  vita_init();
#endif

  bool bRun = true;
#ifdef __vita__
  vita_trace("loop\n");
#endif

  while (bRun) {
    lua_state_unique_ptr L(luaL_newstate());
    if (L == nullptr) {
      std::fprintf(stderr,
                   "Fatal error starting CorsixTH: "
                   "Cannot open Lua state.\n");
      return 0;
    }
#ifdef WITH_TRACY
    tracy::LuaRegister(L.get());
#endif
#ifdef TRACY_ENABLE
    lua_sethook(L.get(), l_tracy_hook, LUA_MASKCALL | LUA_MASKRET, 0);
#endif
    lua_atpanic(L.get(), lua_panic);
    luaL_openlibs(L.get());
#ifdef __vita__
    lua_register(L.get(), "print", vita_lua_print);
    lua_register(L.get(), "vita_persist_stats", vita_persist_stats);
    lua_register(L.get(), "vita_pack_read", vita_pack_read);
    if (luaL_dostring(L.get(), vita_io_open_wrapper) != 0) {
      lua_pop(L.get(), 1);
    }
#endif
    lua_settop(L.get(), 0);
    lua_pushcfunction(L.get(), lua_stacktrace);
    lua_pushcfunction(L.get(), lua_init);

    // Move command line parameters onto the Lua stack
    lua_checkstack(L.get(), argc);
    for (int i = 0; i < argc; ++i) {
      lua_pushstring(L.get(), argv[i]);
    }

#ifdef __vita__
    vita_trace("lua_init\n");
#endif
    if (lua_pcall(L.get(), argc, 0, 1) != 0) {
      const char* err = lua_tostring(L.get(), -1);
#ifdef __vita__
      if (err != nullptr) {
        vita_log(err, std::strlen(err));
        vita_log("\n", 1);
      }
#endif
      if (err != nullptr) {
        std::fprintf(stderr, "%s\n", err);
      } else {
        std::fprintf(stderr,
                     "An error has occurred in CorsixTH:\n"
                     "Uncaught non-string Lua error\n");
      }
      lua_pushcfunction(L.get(), bootstrap_lua_error_report);
      lua_insert(L.get(), -2);
      if (lua_pcall(L.get(), 1, 0, 0) != 0) {
        std::fprintf(stderr, "%s\n", lua_tostring(L.get(), -1));
      }
    }
#ifdef __vita__
    vita_trace("mainloop\n");
#endif
    mainloop(L.get());

    lua_getfield(L.get(), LUA_REGISTRYINDEX, "_RESTART");
    bRun = lua_toboolean(L.get(), -1) != 0;

    // Destroy the lua_State before SDL so that any SDL resource owned by
    // Lua can be freed first.
    L.reset(nullptr);
    while (Mix_QuerySpec(nullptr, nullptr, nullptr)) {
      Mix_CloseAudio();
    }
    SDL_Quit();

    if (bRun) {
      std::printf("\n\nRestarting...\n\n\n");
    }
  }
#ifdef WITH_UPDATE_CHECK
  curl_global_cleanup();
#endif
  return 0;
}
