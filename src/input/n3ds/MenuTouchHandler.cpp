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
#include "../../system/dispatcher.hpp"
#include "TouchHandler.hpp"
#include "gamepad_bgr.h"
#include "keyboard_bgr.h"
#include "touchpad_bgr.h"
#include <Limelight.h>
#include <vector>

static const int button_size_y = 30;

MenuTouchHandler::MenuTouchHandler() {}

void MenuTouchHandler::_handle_touch_down(touchPosition touch) {
    _handle_touch_hold(touch);
}

void MenuTouchHandler::_handle_touch_up(touchPosition touch) {
    if (message != nullptr)
        MessageDispatcher::get_instance()->post_immediate(message.get());
}

void MenuTouchHandler::_handle_touch_hold(touchPosition touch) {
    if (touch.py >= GSP_SCREEN_WIDTH - button_size_y) {
        // Signal to exit the stream
        message = std::make_unique<GenericEventMsg>(MessageType::EXIT_STREAM);
        return;
    }

    N3dsTouchType touch_type = N3dsTouchType::DISABLED;
    const uint8_t *control_image = nullptr;
    if (touch.py <= button_size_y) {
        // Switch to gamepad
        touch_type = N3dsTouchType::GAMEPAD;
        control_image = gamepad_bgr;
    } else if (touch.py <= 2 * button_size_y) {
        // Switch to mouse
        touch_type = N3dsTouchType::MOUSEPAD;
        control_image = touchpad_bgr;
    } else if (touch.py <= 3 * button_size_y) {
        // Switch to keyboard
        touch_type = N3dsTouchType::KEYBOARD;
        control_image = keyboard_bgr;
    } else if (touch.py <= 4 * button_size_y) {
        // Switch to mirror
        touch_type = N3dsTouchType::ABSOLUTE_TOUCH;
    } else if (touch.py <= 5 * button_size_y) {
        // Switch to stretch
        touch_type = N3dsTouchType::DS_TOUCH;
    } else if (touch.py <= 6 * button_size_y) {
        // Switch to magnify
        touch_type = N3dsTouchType::MAGNIFY_TOUCH;
    } else {
        // Undefined button
        message = nullptr;
    }
    // Alert the system about the new touch type
    message = std::make_unique<TouchStateChangedMsg>(touch_type, control_image);
}
