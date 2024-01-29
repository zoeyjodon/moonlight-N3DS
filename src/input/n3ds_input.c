/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "n3ds_input.h"
#include "gamepad_bin.h"
#include "touchpad_bin.h"

#include <3ds.h>
#include <limits.h>
#include <Limelight.h>
#include <stdio.h>

#define QUIT_BUTTONS (PLAY_FLAG|BACK_FLAG|LB_FLAG|RB_FLAG)
#define TOUCH_GAMEPAD_BUTTONS (SPECIAL_FLAG|LS_CLK_FLAG|RS_CLK_FLAG)
#define TOUCH_MOUSEPAD_BUTTONS (BUTTON_LEFT|BUTTON_RIGHT)
#define SUPPORTED_BUTTONS (A_FLAG|B_FLAG|X_FLAG|Y_FLAG|\
  RIGHT_FLAG|LEFT_FLAG|UP_FLAG|DOWN_FLAG|RB_FLAG|LB_FLAG|\
  LS_CLK_FLAG|RS_CLK_FLAG|BACK_FLAG|PLAY_FLAG|SPECIAL_FLAG)
#define N3DS_ANALOG_MAX 150
#define N3DS_C_STICK_MAX 100
#define N3DS_ANALOG_POS_FACTOR 5
#define N3DS_MOUSEPAD_SENSITIVITY 2

typedef struct _GAMEPAD_STATE {
  unsigned char leftTrigger, rightTrigger;
  short leftStickX, leftStickY;
  short rightStickX, rightStickY;
  int buttons, mouse_buttons;
  u16 touchpadX, touchpadY;
  bool touchpad_active, key_active;
  enum N3dsTouchType ttype;
} GAMEPAD_STATE;
static GAMEPAD_STATE gamepad_state, previous_state;

static const int activeGamepadMask = 1;

static void add_gamepad() {
  unsigned short capabilities = 0;
  unsigned char type = LI_CTYPE_NINTENDO;
  LiSendControllerArrivalEvent(0, activeGamepadMask, type, SUPPORTED_BUTTONS, capabilities);
}

static void remove_gamepad() {
  LiSendMultiControllerEvent(0, ~activeGamepadMask, 0, 0, 0, 0, 0, 0, 0);
}

void n3dsinput_init(char* mappings) {
  hidInit();
  add_gamepad();
  gamepad_state.ttype = DEBUG_PRINT;
  gfxSetDoubleBuffering(GFX_BOTTOM, false);
}

void n3dsinput_set_touch(enum N3dsTouchType ttype)
{
  if (gamepad_state.ttype == ttype) {
    return;
  }

  if (gamepad_state.ttype == GAMEPAD) {
    // Clear buttons before updating
    gamepad_state.buttons &= ~TOUCH_GAMEPAD_BUTTONS;
  }

  gamepad_state.ttype = ttype;
  u8* gfxbtmadr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
  switch (gamepad_state.ttype) {
    case GAMEPAD:
      memcpy(gfxbtmadr, gamepad_bin, gamepad_bin_size);
      break;
    case MOUSEPAD:
      memcpy(gfxbtmadr, touchpad_bin, touchpad_bin_size);
      break;
    default:
	    memset(gfxbtmadr, 0, 320 * 240 * 3);
      break;
  }
  gfxFlushBuffers();
  gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

static inline void n3dsinput_cycle_touch() {
  enum N3dsTouchType new_type = gamepad_state.ttype + 1;
  if (new_type == DEBUG_PRINT) {
    new_type = 0;
  }
  n3dsinput_set_touch(new_type);
}

static inline int n3ds_to_li_button(u32 key_in, u32 key_n3ds, int key_li) {
  return ((key_in & key_n3ds) / key_n3ds) * key_li;
}

static inline int n3ds_to_li_buttons(u32 key_n3ds) {
  int li_out = 0;
  li_out |= n3ds_to_li_button(key_n3ds, KEY_A, A_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_B, B_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_SELECT, BACK_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_START, PLAY_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_DRIGHT, RIGHT_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_DLEFT, LEFT_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_DUP, UP_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_DDOWN, DOWN_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_R, RB_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_L, LB_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_X, X_FLAG);
  li_out |= n3ds_to_li_button(key_n3ds, KEY_Y, Y_FLAG);
  return li_out;
}

static inline unsigned char n3ds_to_li_trigger(u32 key_in, u32 key_n3ds) {
  return ((key_in & key_n3ds) / key_n3ds) * 255UL;
}

static inline int scale_n3ds_axis(int axis_n3ds, int axis_max) {
    if (axis_n3ds > axis_max) {
        return SHRT_MAX;
    }
    else if (axis_n3ds < -axis_max) {
        return -SHRT_MAX;
    }
    return (axis_n3ds * SHRT_MAX) / axis_max;
}

static inline bool joystick_state_changed(short before, short after) {
  return (before / N3DS_ANALOG_POS_FACTOR) != (after / N3DS_ANALOG_POS_FACTOR);
}

static inline bool gamepad_state_changed() {
  if ((previous_state.buttons != gamepad_state.buttons) ||
      (previous_state.leftTrigger != gamepad_state.leftTrigger) ||
      (previous_state.rightTrigger != gamepad_state.rightTrigger)) {
    return true;
  }

  if (joystick_state_changed(previous_state.leftStickX, gamepad_state.leftStickX) ||
      joystick_state_changed(previous_state.leftStickY, gamepad_state.leftStickY)){
    return true;
  }

  if (joystick_state_changed(previous_state.rightStickX, gamepad_state.rightStickX) ||
      joystick_state_changed(previous_state.rightStickY, gamepad_state.rightStickY)) {
    return true;
  }

  return false;
}

static inline bool mousepad_state_changed() {
  return previous_state.mouse_buttons != gamepad_state.mouse_buttons;
}

static inline bool change_touchpad_pressed(touchPosition touch) {
  return touch.py >= 205 && touch.px >= 285;
}

static inline int touch_to_li_buttons(touchPosition touch) {
  if (touch.py >= 120) {
    return SPECIAL_FLAG;
  }

  int buttons = 0;
  if (touch.px < 235)
    buttons |= LS_CLK_FLAG;
  if (touch.px > 104)
    buttons |= RS_CLK_FLAG;
  return buttons;
}

static inline int touch_to_mouse_event(touchPosition touch) {
  if (touch.py < 175) {
    gamepad_state.touchpad_active = true;
    gamepad_state.touchpadX = touch.px;
    gamepad_state.touchpadY = touch.py;
    return 0;
  }

  if (touch.py >= 205 && touch.px <= 35) {
    gamepad_state.key_active = true;
    return 0;
  }

  int mouse_buttons = BUTTON_LEFT;
  if (touch.px > 160)
    mouse_buttons = BUTTON_RIGHT;
  return mouse_buttons;
}

static inline void n3dsinput_handle_touch() {
  touchPosition touch;
  hidTouchRead(&touch);
  if (change_touchpad_pressed(touch)) {
    n3dsinput_cycle_touch();
    return;
  }

  switch (gamepad_state.ttype) {
    case GAMEPAD:
      gamepad_state.buttons |= touch_to_li_buttons(touch);
      break;
    case MOUSEPAD:
      gamepad_state.mouse_buttons |= touch_to_mouse_event(touch);
      break;
    default:
      break;
  }
}

int n3dsinput_handle_event() {
  hidScanInput();
  u32 kDown = hidKeysDown();
  u32 kUp = hidKeysUp();
  previous_state = gamepad_state;

  if (kDown) {
    gamepad_state.buttons |= n3ds_to_li_buttons(kDown);
    gamepad_state.leftTrigger |= n3ds_to_li_trigger(kDown, KEY_ZL);
    gamepad_state.rightTrigger |= n3ds_to_li_trigger(kDown, KEY_ZR);
  }
  if (kUp) {
    gamepad_state.buttons &= ~n3ds_to_li_buttons(kUp);
    gamepad_state.leftTrigger &= ~n3ds_to_li_trigger(kUp, KEY_ZL);
    gamepad_state.rightTrigger &= ~n3ds_to_li_trigger(kUp, KEY_ZR);
  }

  if ((gamepad_state.buttons & QUIT_BUTTONS) == QUIT_BUTTONS)
    return 1;

  circlePosition cpad_pos;
  hidCircleRead(&cpad_pos);
  gamepad_state.leftStickX = scale_n3ds_axis(cpad_pos.dx, N3DS_ANALOG_MAX);
  gamepad_state.leftStickY = scale_n3ds_axis(cpad_pos.dy, N3DS_ANALOG_MAX);

  circlePosition cstick_pos;
  hidCstickRead(&cstick_pos);
  gamepad_state.rightStickX = scale_n3ds_axis(cstick_pos.dx, N3DS_C_STICK_MAX);
  gamepad_state.rightStickY = scale_n3ds_axis(cstick_pos.dy, N3DS_C_STICK_MAX);

  if (kDown & KEY_TOUCH) {
    n3dsinput_handle_touch();
    if (gamepad_state.key_active) {
      LiSendKeyboardEvent(0x5B, KEY_ACTION_DOWN, 0);
    }
  }
  else if (kUp & KEY_TOUCH) {
    gamepad_state.buttons &= ~TOUCH_GAMEPAD_BUTTONS;
    gamepad_state.mouse_buttons = 0;
    gamepad_state.touchpad_active = false;
    if (gamepad_state.key_active) {
      LiSendKeyboardEvent(0x5B, KEY_ACTION_UP, 0);
      gamepad_state.key_active = false;
    }
  }
  else if (gamepad_state.touchpad_active) {
    touchPosition touch;
    hidTouchRead(&touch);
    gamepad_state.touchpadX = touch.px;
    gamepad_state.touchpadY = touch.py;
    short deltaX = N3DS_MOUSEPAD_SENSITIVITY * (gamepad_state.touchpadX - previous_state.touchpadX);
    short deltaY = N3DS_MOUSEPAD_SENSITIVITY * (gamepad_state.touchpadY - previous_state.touchpadY);
    LiSendMouseMoveEvent(deltaX, deltaY);
  }

  if (gamepad_state_changed()) {
    LiSendMultiControllerEvent(0, activeGamepadMask, gamepad_state.buttons, gamepad_state.leftTrigger, gamepad_state.rightTrigger, gamepad_state.leftStickX, gamepad_state.leftStickY, gamepad_state.rightStickX, gamepad_state.rightStickY);
  }
  if (mousepad_state_changed()) {
    char mouse_action = BUTTON_ACTION_PRESS;
    int mouse_button = gamepad_state.mouse_buttons;
    if (kUp & KEY_TOUCH) {
      mouse_action = BUTTON_ACTION_RELEASE;
      mouse_button = previous_state.mouse_buttons;
    }
    LiSendMouseButtonEvent(mouse_action, mouse_button);
  }

  return 0;
}
