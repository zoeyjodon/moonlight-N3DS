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
#include "gamepad_bgr.h"
#include "touchpad_bgr.h"

#include <3ds.h>
#include <Limelight.h>
#include <limits.h>
#include <stdio.h>

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
#define N3DS_MOUSEPAD_SENSITIVITY 3

typedef void (*TouchTypeHandler)(touchPosition touch);

typedef struct _GAMEPAD_STATE {
    unsigned char leftTrigger, rightTrigger;
    short leftStickX, leftStickY;
    short rightStickX, rightStickY;
    int buttons, mouse_buttons;
    u16 touchpadX, touchpadY;
    bool touchpad_active, key_active, ds_touch_active;
    enum N3dsTouchType ttype;
    TouchTypeHandler ttype_handler;
    accelVector accel_vector;
    angularRate gyro_rate;
} GAMEPAD_STATE;
static GAMEPAD_STATE gamepad_state, previous_state;

static const int activeGamepadMask = 1;
static float gyro_coeff = 0;
// Note: This was found experimentally and may need a calibration option in settings
static float accel_coeff = 52.0;
bool enable_gyro = false;
bool enable_accel = false;

static void add_gamepad() {
    unsigned short capabilities = LI_CCAP_ACCEL | LI_CCAP_GYRO;
    unsigned char type = LI_CTYPE_NINTENDO;
    LiSendControllerArrivalEvent(0, activeGamepadMask, type, SUPPORTED_BUTTONS,
                                 capabilities);
}

static void remove_gamepad() {
    LiSendMultiControllerEvent(0, ~activeGamepadMask, 0, 0, 0, 0, 0, 0, 0);
}

void n3dsinput_init() {
    hidInit();
    HIDUSER_GetGyroscopeRawToDpsCoefficient(&gyro_coeff);
    add_gamepad();
    gamepad_state.ttype = DISABLED;
    gfxSetDoubleBuffering(GFX_BOTTOM, false);
}

void n3dsinput_cleanup() { remove_gamepad(); }

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
    if ((previous_state.accel_vector.x != gamepad_state.accel_vector.x) ||
        (previous_state.accel_vector.y != gamepad_state.accel_vector.y) ||
        (previous_state.accel_vector.z != gamepad_state.accel_vector.z)) {
        return true;
    }
    return false;
}

static inline bool gyroscope_state_changed() {
    if ((previous_state.gyro_rate.x != gamepad_state.gyro_rate.x) ||
        (previous_state.gyro_rate.y != gamepad_state.gyro_rate.y) ||
        (previous_state.gyro_rate.z != gamepad_state.gyro_rate.z)) {
        return true;
    }
    return false;
}

static inline bool change_touchpad_pressed(touchPosition touch) {
    if (gamepad_state.ttype == DS_TOUCH) {
        return false;
    }
    return touch.py >= 205 && touch.px >= 285;
}

static void touch_gamepad_handler(touchPosition touch) {
    if (touch.py >= 120) {
        gamepad_state.buttons |= SPECIAL_FLAG;
        return;
    }

    if (touch.px < 235)
        gamepad_state.buttons |= LS_CLK_FLAG;
    if (touch.px > 104)
        gamepad_state.buttons |= RS_CLK_FLAG;
}

static void touch_mouse_handler(touchPosition touch) {
    if (touch.py < 175) {
        gamepad_state.touchpad_active = true;
        gamepad_state.touchpadX = touch.px;
        gamepad_state.touchpadY = touch.py;
    } else if (touch.py >= 205 && touch.px <= 35) {
        gamepad_state.key_active = true;
    } else if (touch.px > 160) {
        gamepad_state.mouse_buttons = BUTTON_RIGHT;
    } else {
        gamepad_state.mouse_buttons = BUTTON_LEFT;
    }
}

static void touch_ds_handler(touchPosition touch) {
    gamepad_state.ds_touch_active = true;
    gamepad_state.mouse_buttons = BUTTON_LEFT;
    LiSendMousePositionEvent(touch.px, touch.py + GSP_SCREEN_WIDTH,
                             GSP_SCREEN_HEIGHT_BOTTOM, 2 * GSP_SCREEN_WIDTH);
}

void n3dsinput_set_touch(enum N3dsTouchType ttype) {
    if (gamepad_state.ttype == ttype) {
        return;
    }

    gamepad_state.ttype = ttype;
    u8 *gfxbtmadr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    switch (gamepad_state.ttype) {
    case GAMEPAD:
        memcpy(gfxbtmadr, gamepad_bgr, gamepad_bgr_size);
        gamepad_state.ttype_handler = touch_gamepad_handler;
        break;
    case MOUSEPAD:
        memcpy(gfxbtmadr, touchpad_bgr, touchpad_bgr_size);
        gamepad_state.ttype_handler = touch_mouse_handler;
        break;
    case DS_TOUCH:
        gamepad_state.ttype_handler = touch_ds_handler;
        break;
    default:
        GSPGPU_FramebufferFormat px_fmt = gfxGetScreenFormat(GFX_BOTTOM);
        int pixel_size = gspGetBytesPerPixel(px_fmt);
        memset(gfxbtmadr, 0,
               GSP_SCREEN_HEIGHT_BOTTOM * GSP_SCREEN_WIDTH * pixel_size);
        gamepad_state.ttype_handler = NULL;
        break;
    }
    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

static inline void n3dsinput_cycle_touch() {
    enum N3dsTouchType new_type = gamepad_state.ttype + 1;
    if (new_type == DISABLED) {
        new_type = 0;
    }
    n3dsinput_set_touch(new_type);
}

static inline void n3dsinput_handle_touch() {
    if (gamepad_state.ttype_handler == NULL) {
        // Debug mode
        return;
    }

    touchPosition touch;
    hidTouchRead(&touch);
    if (change_touchpad_pressed(touch)) {
        n3dsinput_cycle_touch();
    } else {
        gamepad_state.ttype_handler(touch);
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
    gamepad_state.rightStickX =
        scale_n3ds_axis(cstick_pos.dx, N3DS_C_STICK_MAX);
    gamepad_state.rightStickY =
        scale_n3ds_axis(cstick_pos.dy, N3DS_C_STICK_MAX);

    if (kDown & KEY_TOUCH) {
        n3dsinput_handle_touch();
        if (gamepad_state.key_active) {
            LiSendKeyboardEvent(0x5B, KEY_ACTION_DOWN, 0);
        } else if (gamepad_state.mouse_buttons) {
            LiSendMouseButtonEvent(BUTTON_ACTION_PRESS,
                                   gamepad_state.mouse_buttons);
        }
    } else if (kUp & KEY_TOUCH) {
        gamepad_state.buttons &= ~TOUCH_GAMEPAD_BUTTONS;
        gamepad_state.touchpad_active = false;
        gamepad_state.ds_touch_active = false;
        if (gamepad_state.key_active) {
            LiSendKeyboardEvent(0x5B, KEY_ACTION_UP, 0);
            gamepad_state.key_active = false;
        } else if (gamepad_state.mouse_buttons) {
            LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE,
                                   gamepad_state.mouse_buttons);
            gamepad_state.mouse_buttons = 0;
        }
    } else if (gamepad_state.touchpad_active) {
        touchPosition touch;
        hidTouchRead(&touch);
        gamepad_state.touchpadX = touch.px;
        gamepad_state.touchpadY = touch.py;
        short deltaX = N3DS_MOUSEPAD_SENSITIVITY *
                       (gamepad_state.touchpadX - previous_state.touchpadX);
        short deltaY = N3DS_MOUSEPAD_SENSITIVITY *
                       (gamepad_state.touchpadY - previous_state.touchpadY);
        LiSendMouseMoveEvent(deltaX, deltaY);
    } else if (gamepad_state.ds_touch_active) {
        touchPosition touch;
        hidTouchRead(&touch);
        LiSendMousePositionEvent(touch.px, touch.py + GSP_SCREEN_WIDTH,
                                 GSP_SCREEN_HEIGHT_BOTTOM,
                                 2 * GSP_SCREEN_WIDTH);
    }

    if (gamepad_state_changed()) {
        LiSendMultiControllerEvent(
            0, activeGamepadMask, gamepad_state.buttons,
            gamepad_state.leftTrigger, gamepad_state.rightTrigger,
            gamepad_state.leftStickX, gamepad_state.leftStickY,
            gamepad_state.rightStickX, gamepad_state.rightStickY);
    }

    if (enable_accel) {
        hidAccelRead(&gamepad_state.accel_vector);
        if (accelerometer_state_changed()) {
            LiSendControllerMotionEvent(
                0, LI_MOTION_TYPE_ACCEL,
                gamepad_state.accel_vector.x / accel_coeff,
                gamepad_state.accel_vector.y / accel_coeff,
                gamepad_state.accel_vector.z / accel_coeff);
        }
    }

    if (enable_gyro) {
        hidGyroRead(&gamepad_state.gyro_rate);
        if (gyroscope_state_changed()) {
            // Convert rad/s to deg/s
            LiSendControllerMotionEvent(0, LI_MOTION_TYPE_GYRO,
                                        gamepad_state.gyro_rate.x * gyro_coeff,
                                        gamepad_state.gyro_rate.y * gyro_coeff,
                                        gamepad_state.gyro_rate.z * gyro_coeff);
        }
    }

    return 0;
}
