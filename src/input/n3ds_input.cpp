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

#include "n3ds_input.hpp"

#include <3ds.h>
#include <Limelight.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define QUIT_BUTTONS (PLAY_FLAG | BACK_FLAG | LB_FLAG | RB_FLAG)
#define TOUCH_GAMEPAD_BUTTONS (SPECIAL_FLAG | LS_CLK_FLAG | RS_CLK_FLAG)
#define TOUCH_MOUSEPAD_BUTTONS (BUTTON_LEFT | BUTTON_RIGHT)
#define SUPPORTED_BUTTONS                                                      \
    (A_FLAG | B_FLAG | X_FLAG | Y_FLAG | RIGHT_FLAG | LEFT_FLAG | UP_FLAG |    \
     DOWN_FLAG | RB_FLAG | LB_FLAG | LS_CLK_FLAG | RS_CLK_FLAG | BACK_FLAG |   \
     PLAY_FLAG | SPECIAL_FLAG)
#define N3DS_ANALOG_MAX 150
#define N3DS_C_STICK_MAX 100
#define N3DS_ANALOG_POS_FACTOR 5

static GAMEPAD_STATE gamepad_state, previous_state;
std::unique_ptr<N3dsTouchscreenInput> touch_handler = nullptr;

static const int activeGamepadMask = 1;
static float gyro_coeff = 0;
// Note: This was found experimentally and may need a calibration option in
// settings
static float accel_coeff = 52.0;
bool enable_gyro = false;
bool enable_accel = false;
bool use_triggers_for_mouse = false;

static u32 SWAP_A = KEY_A;
static u32 SWAP_B = KEY_B;
static u32 SWAP_X = KEY_X;
static u32 SWAP_Y = KEY_Y;

static u32 SWAP_L = KEY_L;
static u32 SWAP_R = KEY_R;
static u32 SWAP_ZL = KEY_ZL;
static u32 SWAP_ZR = KEY_ZR;

static void add_gamepad() {
    unsigned short capabilities = LI_CCAP_ACCEL | LI_CCAP_GYRO;
    unsigned char type = LI_CTYPE_NINTENDO;
    LiSendControllerArrivalEvent(0, activeGamepadMask, type, SUPPORTED_BUTTONS,
                                 capabilities);
}

static void remove_gamepad() {
    LiSendMultiControllerEvent(0, ~activeGamepadMask, 0, 0, 0, 0, 0, 0, 0);
}

void n3dsinput_init(N3dsTouchType touch_type, bool swap_face_buttons,
                    bool swap_triggers_and_shoulders,
                    bool use_triggers_for_mouse_in) {
    hidInit();
    HIDUSER_GetGyroscopeRawToDpsCoefficient(&gyro_coeff);
    add_gamepad();
    use_triggers_for_mouse = use_triggers_for_mouse_in;

    if (swap_face_buttons) {
        SWAP_A = KEY_B;
        SWAP_B = KEY_A;
        SWAP_X = KEY_Y;
        SWAP_Y = KEY_X;
    } else {
        SWAP_A = KEY_A;
        SWAP_B = KEY_B;
        SWAP_X = KEY_X;
        SWAP_Y = KEY_Y;
    }

    if (swap_triggers_and_shoulders) {
        SWAP_L = KEY_ZL;
        SWAP_R = KEY_ZR;
        SWAP_ZL = KEY_L;
        SWAP_ZR = KEY_R;
    } else {
        SWAP_L = KEY_L;
        SWAP_R = KEY_R;
        SWAP_ZL = KEY_ZL;
        SWAP_ZR = KEY_ZR;
    }

    touch_handler =
        std::make_unique<N3dsTouchscreenInput>(&gamepad_state, touch_type);
}

void n3dsinput_cleanup() {
    remove_gamepad();
    gamepad_state = GAMEPAD_STATE();
    previous_state = GAMEPAD_STATE();
    touch_handler = nullptr;
}

static inline int n3ds_to_li_button(u32 key_in, u32 key_n3ds, int key_li) {
    return ((key_in & key_n3ds) / key_n3ds) * key_li;
}

static inline int n3ds_to_li_buttons(u32 key_n3ds) {
    int li_out = 0;
    li_out |= n3ds_to_li_button(key_n3ds, SWAP_A, A_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, SWAP_B, B_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_SELECT, BACK_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_START, PLAY_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DRIGHT, RIGHT_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DLEFT, LEFT_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DUP, UP_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DDOWN, DOWN_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, SWAP_R, RB_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, SWAP_L, LB_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, SWAP_X, X_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, SWAP_Y, Y_FLAG);
    return li_out;
}

static inline unsigned char n3ds_to_li_trigger(u32 key_in, u32 key_n3ds) {
    return ((key_in & key_n3ds) / key_n3ds) * 255UL;
}

static inline int scale_n3ds_axis(int axis_n3ds, int axis_max) {
    if (axis_n3ds > axis_max) {
        return SHRT_MAX;
    } else if (axis_n3ds < -axis_max) {
        return -SHRT_MAX;
    }
    return (axis_n3ds * SHRT_MAX) / axis_max;
}

static inline bool joystick_state_changed(short before, short after) {
    return (before / N3DS_ANALOG_POS_FACTOR) !=
           (after / N3DS_ANALOG_POS_FACTOR);
}

static inline bool gamepad_state_changed() {
    if ((previous_state.buttons != gamepad_state.buttons) ||
        (previous_state.leftTrigger != gamepad_state.leftTrigger) ||
        (previous_state.rightTrigger != gamepad_state.rightTrigger)) {
        return true;
    }

    if (joystick_state_changed(previous_state.leftStickX,
                               gamepad_state.leftStickX) ||
        joystick_state_changed(previous_state.leftStickY,
                               gamepad_state.leftStickY)) {
        return true;
    }

    if (joystick_state_changed(previous_state.rightStickX,
                               gamepad_state.rightStickX) ||
        joystick_state_changed(previous_state.rightStickY,
                               gamepad_state.rightStickY)) {
        return true;
    }

    return false;
}

static inline bool accelerometer_state_changed() {
    if ((previous_state.accel_vector_x != gamepad_state.accel_vector_x) ||
        (previous_state.accel_vector_y != gamepad_state.accel_vector_y) ||
        (previous_state.accel_vector_z != gamepad_state.accel_vector_z)) {
        return true;
    }
    return false;
}

static inline bool gyroscope_state_changed() {
    if ((previous_state.gyro_rate_x != gamepad_state.gyro_rate_x) ||
        (previous_state.gyro_rate_y != gamepad_state.gyro_rate_y) ||
        (previous_state.gyro_rate_z != gamepad_state.gyro_rate_z)) {
        return true;
    }
    return false;
}

int n3dsinput_handle_event() {
    hidScanInput();
    u32 kDown = hidKeysDown();
    u32 kUp = hidKeysUp();
    previous_state = gamepad_state;

    touch_handler->n3dsinput_handle_touch(kDown, kUp);

    if (kDown) {
        gamepad_state.buttons |= n3ds_to_li_buttons(kDown);
        gamepad_state.leftTrigger |= n3ds_to_li_trigger(kDown, SWAP_ZL);
        gamepad_state.rightTrigger |= n3ds_to_li_trigger(kDown, SWAP_ZR);
    }
    if (kUp) {
        gamepad_state.buttons &= ~n3ds_to_li_buttons(kUp);
        gamepad_state.leftTrigger &= ~n3ds_to_li_trigger(kUp, SWAP_ZL);
        gamepad_state.rightTrigger &= ~n3ds_to_li_trigger(kUp, SWAP_ZR);
    }

    if ((gamepad_state.buttons & QUIT_BUTTONS) == QUIT_BUTTONS)
        return 1;

    circlePosition cpad_pos;
    hidCircleRead(&cpad_pos);
    gamepad_state.leftStickX = scale_n3ds_axis(cpad_pos.dx, N3DS_ANALOG_MAX);
    gamepad_state.leftStickY = scale_n3ds_axis(cpad_pos.dy, N3DS_ANALOG_MAX);

    circlePosition cstick_pos;
    hidCstickRead(&cstick_pos);
    gamepad_state.rightStickX =
        scale_n3ds_axis(cstick_pos.dx, N3DS_C_STICK_MAX);
    gamepad_state.rightStickY =
        scale_n3ds_axis(cstick_pos.dy, N3DS_C_STICK_MAX);

    if (gamepad_state_changed()) {
        if (use_triggers_for_mouse) {
            if (previous_state.leftTrigger != gamepad_state.leftTrigger) {
                LiSendMouseButtonEvent(gamepad_state.leftTrigger
                                           ? BUTTON_ACTION_PRESS
                                           : BUTTON_ACTION_RELEASE,
                                       BUTTON_LEFT);
            }
            if (previous_state.rightTrigger != gamepad_state.rightTrigger) {
                LiSendMouseButtonEvent(gamepad_state.rightTrigger
                                           ? BUTTON_ACTION_PRESS
                                           : BUTTON_ACTION_RELEASE,
                                       BUTTON_RIGHT);
            }
            LiSendMultiControllerEvent(
                0, activeGamepadMask, gamepad_state.buttons, 0, 0,
                gamepad_state.leftStickX, gamepad_state.leftStickY,
                gamepad_state.rightStickX, gamepad_state.rightStickY);
        } else {
            LiSendMultiControllerEvent(
                0, activeGamepadMask, gamepad_state.buttons,
                gamepad_state.leftTrigger, gamepad_state.rightTrigger,
                gamepad_state.leftStickX, gamepad_state.leftStickY,
                gamepad_state.rightStickX, gamepad_state.rightStickY);
        }
    }

    if (enable_accel) {
        accelVector accel_vector;
        hidAccelRead(&accel_vector);
        gamepad_state.accel_vector_x = trunc(accel_vector.x / accel_coeff);
        gamepad_state.accel_vector_y = trunc(accel_vector.y / accel_coeff);
        gamepad_state.accel_vector_z = trunc(accel_vector.z / accel_coeff);
        if (accelerometer_state_changed()) {
            LiSendControllerMotionEvent(
                0, LI_MOTION_TYPE_ACCEL, gamepad_state.accel_vector_x,
                gamepad_state.accel_vector_y, gamepad_state.accel_vector_z);
        }
    }

    if (enable_gyro) {
        angularRate gyro_rate;
        hidGyroRead(&gyro_rate);
        gamepad_state.gyro_rate_x = trunc(-1 * gyro_rate.x / gyro_coeff);
        gamepad_state.gyro_rate_y = trunc(gyro_rate.y / gyro_coeff);
        gamepad_state.gyro_rate_z = trunc(-1 * gyro_rate.z / gyro_coeff);
        if (gyroscope_state_changed()) {
            LiSendControllerMotionEvent(
                0, LI_MOTION_TYPE_GYRO, gamepad_state.gyro_rate_x,
                gamepad_state.gyro_rate_y, gamepad_state.gyro_rate_z);
        }
    }

    return 0;
}
