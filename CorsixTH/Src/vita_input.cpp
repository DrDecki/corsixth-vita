#include "config.h"

#ifdef __vita__

#include "vita_input.h"

#include <SDL.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int screen_width = 960;
constexpr int screen_height = 544;
constexpr float deadzone = 0.2f;
constexpr float cursor_speed = 11.0f;
constexpr int zoom_interval = 4;

bool initialised = false;
float cursor_x = screen_width / 2.0f;
float cursor_y = screen_height / 2.0f;
uint32_t prev_buttons = 0;
bool keys_down[4] = {false, false, false, false};
int zoom_counter = 0;
bool touch_down = false;

const SDL_Keycode arrow_keys[4] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};

float read_axis(unsigned char value) {
  float f = (static_cast<float>(value) - 128.0f) / 127.0f;
  float a = std::fabs(f);
  if (a < deadzone) return 0.0f;
  a = std::min(1.0f, (a - deadzone) / (1.0f - deadzone));
  return f < 0.0f ? -a : a;
}

void push_motion(int x, int y, int dx, int dy) {
  SDL_Event e{};
  e.type = SDL_MOUSEMOTION;
  e.motion.timestamp = SDL_GetTicks();
  e.motion.x = x;
  e.motion.y = y;
  e.motion.xrel = dx;
  e.motion.yrel = dy;
  SDL_PushEvent(&e);
}

void push_button(Uint8 button, bool down) {
  SDL_Event e{};
  e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
  e.button.timestamp = SDL_GetTicks();
  e.button.button = button;
  e.button.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.button.clicks = 1;
  e.button.x = static_cast<int>(cursor_x);
  e.button.y = static_cast<int>(cursor_y);
  SDL_PushEvent(&e);
}

void push_key(SDL_Keycode key, bool down) {
  SDL_Event e{};
  e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
  e.key.timestamp = SDL_GetTicks();
  e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
  e.key.keysym.sym = key;
  e.key.keysym.scancode = SDL_GetScancodeFromKey(key);
  e.key.keysym.mod = KMOD_NONE;
  SDL_PushEvent(&e);
}

void push_wheel(int y) {
  SDL_Event e{};
  e.type = SDL_MOUSEWHEEL;
  e.wheel.timestamp = SDL_GetTicks();
  e.wheel.y = y;
  e.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
  SDL_PushEvent(&e);
}

bool pressed(uint32_t buttons, uint32_t mask) {
  return (buttons & mask) && !(prev_buttons & mask);
}

bool released(uint32_t buttons, uint32_t mask) {
  return !(buttons & mask) && (prev_buttons & mask);
}

}  // namespace

void vita_input_sync_cursor(int x, int y) {
  cursor_x = static_cast<float>(x);
  cursor_y = static_cast<float>(y);
}

void vita_input_update() {
  if (!initialised) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT,
                             SCE_TOUCH_SAMPLING_STATE_START);
    initialised = true;
  }

  SceTouchData touch{};
  if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0) {
    if (touch.reportNum > 0) {
      int old_x = static_cast<int>(cursor_x);
      int old_y = static_cast<int>(cursor_y);
      int x = std::clamp(touch.report[0].x / 2, 0, screen_width - 1);
      int y = std::clamp(touch.report[0].y / 2, 0, screen_height - 1);
      cursor_x = static_cast<float>(x);
      cursor_y = static_cast<float>(y);
      if (x != old_x || y != old_y) push_motion(x, y, x - old_x, y - old_y);
      if (!touch_down) {
        push_button(SDL_BUTTON_LEFT, true);
        touch_down = true;
      }
    } else if (touch_down) {
      push_button(SDL_BUTTON_LEFT, false);
      touch_down = false;
    }
  }

  SceCtrlData pad{};
  if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0) return;
  uint32_t buttons = pad.buttons;

  float lx = read_axis(pad.lx);
  float ly = read_axis(pad.ly);
  if (lx != 0.0f || ly != 0.0f) {
    int old_x = static_cast<int>(cursor_x);
    int old_y = static_cast<int>(cursor_y);
    cursor_x += lx * std::fabs(lx) * cursor_speed;
    cursor_y += ly * std::fabs(ly) * cursor_speed;
    cursor_x = std::clamp(cursor_x, 0.0f, screen_width - 1.0f);
    cursor_y = std::clamp(cursor_y, 0.0f, screen_height - 1.0f);
    int new_x = static_cast<int>(cursor_x);
    int new_y = static_cast<int>(cursor_y);
    if (new_x != old_x || new_y != old_y) {
      push_motion(new_x, new_y, new_x - old_x, new_y - old_y);
    }
  }

  if (pressed(buttons, SCE_CTRL_CROSS)) push_button(SDL_BUTTON_LEFT, true);
  if (released(buttons, SCE_CTRL_CROSS)) push_button(SDL_BUTTON_LEFT, false);
  if (pressed(buttons, SCE_CTRL_CIRCLE)) push_button(SDL_BUTTON_RIGHT, true);
  if (released(buttons, SCE_CTRL_CIRCLE)) push_button(SDL_BUTTON_RIGHT, false);

  if (pressed(buttons, SCE_CTRL_START)) push_key(SDLK_ESCAPE, true);
  if (released(buttons, SCE_CTRL_START)) push_key(SDLK_ESCAPE, false);
  if (pressed(buttons, SCE_CTRL_SELECT)) push_key(SDLK_p, true);
  if (released(buttons, SCE_CTRL_SELECT)) push_key(SDLK_p, false);

  float rx = read_axis(pad.rx);
  float ry = read_axis(pad.ry);
  bool want[4] = {
      (buttons & SCE_CTRL_UP) || ry < -0.3f,
      (buttons & SCE_CTRL_DOWN) || ry > 0.3f,
      (buttons & SCE_CTRL_LEFT) || rx < -0.3f,
      (buttons & SCE_CTRL_RIGHT) || rx > 0.3f,
  };
  for (int i = 0; i < 4; ++i) {
    if (want[i] != keys_down[i]) {
      push_key(arrow_keys[i], want[i]);
      keys_down[i] = want[i];
    }
  }

  bool zoom_in = buttons & SCE_CTRL_RTRIGGER;
  bool zoom_out = buttons & SCE_CTRL_LTRIGGER;
  if (zoom_in != zoom_out) {
    if (zoom_counter == 0) push_wheel(zoom_in ? 1 : -1);
    zoom_counter = (zoom_counter + 1) % zoom_interval;
  } else {
    zoom_counter = 0;
  }

  prev_buttons = buttons;
}

#endif
