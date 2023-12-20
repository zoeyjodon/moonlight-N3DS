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

#include "sdl.h"
#include "../sdl_main.h"

#include <3ds.h>
#include <limits.h>
#include <Limelight.h>
#include <stdio.h>

#define QUIT_BUTTONS (PLAY_FLAG|BACK_FLAG|LB_FLAG|RB_FLAG)
#define SUPPORTED_BUTTONS (A_FLAG|B_FLAG|X_FLAG|Y_FLAG|\
  RIGHT_FLAG|LEFT_FLAG|UP_FLAG|DOWN_FLAG|RB_FLAG|LB_FLAG|\
  BACK_FLAG|PLAY_FLAG|TOUCHPAD_FLAG)

typedef struct _GAMEPAD_STATE {
  unsigned char leftTrigger, rightTrigger;
  short leftStickX, leftStickY;
  short rightStickX, rightStickY;
  int buttons;
} GAMEPAD_STATE;
static GAMEPAD_STATE gamepad_state;

static int activeGamepadMask = 1;

static void add_gamepad() {
  unsigned short capabilities = 0;
  unsigned char type = LI_CTYPE_UNKNOWN;
  LiSendControllerArrivalEvent(0, activeGamepadMask, type, SUPPORTED_BUTTONS, capabilities);
}

static void remove_gamepad() {
  LiSendMultiControllerEvent(0, ~activeGamepadMask, 0, 0, 0, 0, 0, 0, 0);
}

void sdlinput_init(char* mappings) {
  hidInit();
  add_gamepad();
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
  li_out |= n3ds_to_li_button(key_n3ds, KEY_TOUCH, TOUCHPAD_FLAG);
  return li_out;
}

static inline unsigned char n3ds_to_li_trigger(u32 key_in, u32 key_n3ds) {
  return ((key_in & key_n3ds) / key_n3ds) * 255UL;
}

static inline int scale_n3ds_axis(int axis_n3ds) {
    if (axis_n3ds > 160) {
        return SHRT_MAX;
    }
    else if (axis_n3ds < -160) {
        return -SHRT_MAX;
    }
    return (axis_n3ds * SHRT_MAX) / 160;
}

int sdlinput_handle_event(SDL_Event* event) {
  hidScanInput();
  u32 kDown = hidKeysDown();
  u32 kUp = hidKeysUp();

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
    return SDL_QUIT_APPLICATION;

  circlePosition cpad_pos;
  hidCircleRead(&cpad_pos);
  gamepad_state.leftStickX = scale_n3ds_axis(cpad_pos.dx);
  gamepad_state.leftStickY = scale_n3ds_axis(cpad_pos.dy);

  circlePosition cstick_pos;
  hidCstickRead(&cstick_pos);
  gamepad_state.rightStickX = scale_n3ds_axis(cstick_pos.dx);
  gamepad_state.rightStickY = scale_n3ds_axis(cstick_pos.dy);

  LiSendMultiControllerEvent(0, activeGamepadMask, gamepad_state.buttons, gamepad_state.leftTrigger, gamepad_state.rightTrigger, gamepad_state.leftStickX, gamepad_state.leftStickY, gamepad_state.rightStickX, gamepad_state.rightStickY);

  // return SDL_QUIT_APPLICATION;
  return SDL_NOTHING;
}
