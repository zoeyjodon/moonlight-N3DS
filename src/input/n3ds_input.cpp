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
#include "../system/dispatcher.hpp"
#include "touch/TouchHandler.hpp"

#include <3ds.h>
#include <Limelight.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TOUCH_GAMEPAD_BUTTONS (SPECIAL_FLAG | LS_CLK_FLAG | RS_CLK_FLAG)
#define TOUCH_MOUSEPAD_BUTTONS (BUTTON_LEFT | BUTTON_RIGHT)
#define SUPPORTED_BUTTONS                                                      \
    (A_FLAG | B_FLAG | X_FLAG | Y_FLAG | RIGHT_FLAG | LEFT_FLAG | UP_FLAG |    \
     DOWN_FLAG | RB_FLAG | LB_FLAG | LS_CLK_FLAG | RS_CLK_FLAG | BACK_FLAG |   \
     PLAY_FLAG | SPECIAL_FLAG)
#define N3DS_ANALOG_MAX 150
#define N3DS_C_STICK_MAX 100
#define N3DS_ANALOG_POS_FACTOR 5

// The 3DS only has one controller; set it to P1
#define CONTROLLER_NUMBER 0
#define ACTIVE_GAMEPAD_MASK 1

N3dsInput::N3dsInput(int image_width, int image_height, bool swap_face_buttons,
                     bool swap_triggers_and_shoulders,
                     bool use_triggers_for_mouse_in) {
    hidInit();
    HIDUSER_GetGyroscopeRawToDpsCoefficient(&gyro_coeff);
    _add_gamepad();
    use_triggers_for_mouse = use_triggers_for_mouse_in;

    // Ternary setup is less efficient, but more readable.
    // We can afford the minor performance cost here.
    CUSTOM_KEY_A = swap_face_buttons ? KEY_B : KEY_A;
    CUSTOM_KEY_B = swap_face_buttons ? KEY_A : KEY_B;
    CUSTOM_KEY_X = swap_face_buttons ? KEY_Y : KEY_X;
    CUSTOM_KEY_Y = swap_face_buttons ? KEY_X : KEY_Y;

    CUSTOM_KEY_L = swap_triggers_and_shoulders ? KEY_ZL : KEY_L;
    CUSTOM_KEY_R = swap_triggers_and_shoulders ? KEY_ZR : KEY_R;
    CUSTOM_KEY_ZL = swap_triggers_and_shoulders ? KEY_L : KEY_ZL;
    CUSTOM_KEY_ZR = swap_triggers_and_shoulders ? KEY_R : KEY_ZR;

    aptSetHomeAllowed(false);
    touch_handler = std::make_unique<N3dsTouchscreenInput>(
        &gamepad_state, image_width, image_height);

    auto pDispatcher = MessageDispatcher::get_instance();
    pDispatcher->subscribe(MessageType::ENABLE_ACCEL, this);
    pDispatcher->subscribe(MessageType::ENABLE_GYRO, this);
}

N3dsInput::~N3dsInput() {
    auto pDispatcher = MessageDispatcher::get_instance();
    pDispatcher->unsubscribe(MessageType::ENABLE_ACCEL, this);
    pDispatcher->unsubscribe(MessageType::ENABLE_GYRO, this);

    _remove_gamepad();
    gamepad_state = GAMEPAD_STATE();
    previous_state = GAMEPAD_STATE();
    touch_handler = nullptr;
    aptSetHomeAllowed(true);
    printf("Input handler shutdown successfully\n");
}

void N3dsInput::accept(IMessage *msg) {
    if (msg->getMessageType() == MessageType::ENABLE_ACCEL) {
        enable_accel.store(true);
    } else if (msg->getMessageType() == MessageType::ENABLE_GYRO) {
        enable_gyro.store(true);
    }
}

void N3dsInput::_add_gamepad() {
    unsigned short capabilities = LI_CCAP_ACCEL | LI_CCAP_GYRO;
    unsigned char type = LI_CTYPE_NINTENDO;
    LiSendControllerArrivalEvent(CONTROLLER_NUMBER, ACTIVE_GAMEPAD_MASK, type,
                                 SUPPORTED_BUTTONS, capabilities);
}

void N3dsInput::_remove_gamepad() {
    LiSendMultiControllerEvent(CONTROLLER_NUMBER, ~ACTIVE_GAMEPAD_MASK, 0, 0, 0,
                               0, 0, 0, 0);
}

static inline int n3ds_to_li_button(u32 key_in, u32 key_n3ds, int key_li) {
    return (key_in & key_n3ds) ? key_li : 0;
}

int N3dsInput::_n3ds_to_li_buttons(u32 key_n3ds) {
    int li_out = 0;
    li_out |= n3ds_to_li_button(key_n3ds, CUSTOM_KEY_A, A_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, CUSTOM_KEY_B, B_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_SELECT, BACK_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_START, PLAY_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DRIGHT, RIGHT_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DLEFT, LEFT_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DUP, UP_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, KEY_DDOWN, DOWN_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, CUSTOM_KEY_R, RB_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, CUSTOM_KEY_L, LB_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, CUSTOM_KEY_X, X_FLAG);
    li_out |= n3ds_to_li_button(key_n3ds, CUSTOM_KEY_Y, Y_FLAG);
    return li_out;
}

static inline uint8_t n3ds_to_li_trigger(u32 key_in, u32 key_n3ds) {
    return (key_in & key_n3ds) ? 255UL : 0UL;
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

bool N3dsInput::_gamepad_state_changed() {
    return (previous_state.buttons != gamepad_state.buttons) ||
           (previous_state.leftTrigger != gamepad_state.leftTrigger) ||
           (previous_state.rightTrigger != gamepad_state.rightTrigger) ||
           joystick_state_changed(previous_state.leftStickX,
                                  gamepad_state.leftStickX) ||
           joystick_state_changed(previous_state.leftStickY,
                                  gamepad_state.leftStickY) ||
           joystick_state_changed(previous_state.rightStickX,
                                  gamepad_state.rightStickX) ||
           joystick_state_changed(previous_state.rightStickY,
                                  gamepad_state.rightStickY);
}

bool N3dsInput::_accelerometer_state_changed() {
    return (previous_state.accel_vector_x != gamepad_state.accel_vector_x) ||
           (previous_state.accel_vector_y != gamepad_state.accel_vector_y) ||
           (previous_state.accel_vector_z != gamepad_state.accel_vector_z);
}

bool N3dsInput::_gyroscope_state_changed() {
    return (previous_state.gyro_rate_x != gamepad_state.gyro_rate_x) ||
           (previous_state.gyro_rate_y != gamepad_state.gyro_rate_y) ||
           (previous_state.gyro_rate_z != gamepad_state.gyro_rate_z);
}

void N3dsInput::force_touchscreen_menu() {
    auto message =
        std::make_shared<TouchStateChangedMsg>(N3dsTouchType::MENU_TOUCH);
    MessageDispatcher::get_instance()->post(message);
}

void N3dsInput::n3dsinput_handle_event() {
    hidScanInput();
    u32 kDown = hidKeysDown();
    u32 kUp = hidKeysUp();
    previous_state = gamepad_state;

    touch_handler->n3dsinput_handle_touch(kDown, kUp);

    if (kDown & ~KEY_TOUCH) {
        gamepad_state.buttons |= _n3ds_to_li_buttons(kDown);
        gamepad_state.leftTrigger |= n3ds_to_li_trigger(kDown, CUSTOM_KEY_ZL);
        gamepad_state.rightTrigger |= n3ds_to_li_trigger(kDown, CUSTOM_KEY_ZR);
    }
    if (kUp & ~KEY_TOUCH) {
        gamepad_state.buttons &= ~_n3ds_to_li_buttons(kUp);
        gamepad_state.leftTrigger &= ~n3ds_to_li_trigger(kUp, CUSTOM_KEY_ZL);
        gamepad_state.rightTrigger &= ~n3ds_to_li_trigger(kUp, CUSTOM_KEY_ZR);
    }

    // Use the HOME button to open the menu
    if (aptCheckHomePressRejected()) {
        if (!menu_active) {
            force_touchscreen_menu();
            menu_active = true;
        }
        return;
    } else {
        menu_active = false;
    }

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

    if (_gamepad_state_changed()) {
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
                CONTROLLER_NUMBER, ACTIVE_GAMEPAD_MASK, gamepad_state.buttons,
                0, 0, gamepad_state.leftStickX, gamepad_state.leftStickY,
                gamepad_state.rightStickX, gamepad_state.rightStickY);
        } else {
            LiSendMultiControllerEvent(
                CONTROLLER_NUMBER, ACTIVE_GAMEPAD_MASK, gamepad_state.buttons,
                gamepad_state.leftTrigger, gamepad_state.rightTrigger,
                gamepad_state.leftStickX, gamepad_state.leftStickY,
                gamepad_state.rightStickX, gamepad_state.rightStickY);
        }
    }

    if (enable_accel.load()) {
        accelVector accel_vector;
        hidAccelRead(&accel_vector);
        gamepad_state.accel_vector_x = trunc(accel_vector.x / accel_coeff);
        gamepad_state.accel_vector_y = trunc(accel_vector.y / accel_coeff);
        gamepad_state.accel_vector_z = trunc(accel_vector.z / accel_coeff);
        if (_accelerometer_state_changed()) {
            LiSendControllerMotionEvent(CONTROLLER_NUMBER, LI_MOTION_TYPE_ACCEL,
                                        gamepad_state.accel_vector_x,
                                        gamepad_state.accel_vector_y,
                                        gamepad_state.accel_vector_z);
        }
    }

    if (enable_gyro.load()) {
        angularRate gyro_rate;
        hidGyroRead(&gyro_rate);
        gamepad_state.gyro_rate_x = trunc(-1 * gyro_rate.x / gyro_coeff);
        gamepad_state.gyro_rate_y = trunc(gyro_rate.y / gyro_coeff);
        gamepad_state.gyro_rate_z = trunc(-1 * gyro_rate.z / gyro_coeff);
        if (_gyroscope_state_changed()) {
            LiSendControllerMotionEvent(CONTROLLER_NUMBER, LI_MOTION_TYPE_GYRO,
                                        gamepad_state.gyro_rate_x,
                                        gamepad_state.gyro_rate_y,
                                        gamepad_state.gyro_rate_z);
        }
    }

    return;
}
