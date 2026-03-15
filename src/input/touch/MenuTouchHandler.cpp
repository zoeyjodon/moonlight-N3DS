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
#include <Limelight.h>
#include <memory>
#include <vector>

static const int button_size_y = 60;
static const int button_size_x = 160;

// Map the Row and Column numbers of each button to their corresponding
// Touch Type Setting
static std::map<int, std::map<int, N3dsTouchType>> button_map{
    {0, {{0, N3dsTouchType::GAMEPAD}, {1, N3dsTouchType::MOUSEPAD}}},
    {1, {{0, N3dsTouchType::KEYBOARD}, {1, N3dsTouchType::ABSOLUTE_TOUCH}}},
    {2, {{0, N3dsTouchType::DS_TOUCH}, {1, N3dsTouchType::MAGNIFY_TOUCH}}},
    {3, {{0, N3dsTouchType::DEBUG_TOUCH}, {1, N3dsTouchType::DISABLED}}},
};

MenuTouchHandler::MenuTouchHandler() { aptSetHomeAllowed(true); }

MenuTouchHandler::~MenuTouchHandler() { aptSetHomeAllowed(false); }

void MenuTouchHandler::_handle_touch_down(touchPosition touch) {
    _handle_touch_hold(touch);
}

void MenuTouchHandler::_handle_touch_up(touchPosition touch) {
    if (message != nullptr)
        MessageDispatcher::get_instance()->post(message);
}

void MenuTouchHandler::_handle_touch_hold(touchPosition touch) {
    touch.py = touch.py < GSP_SCREEN_WIDTH ? touch.py : (touch.py - 1);
    touch.px = touch.px < GSP_SCREEN_HEIGHT_BOTTOM ? touch.px : (touch.px - 1);
    int round_y = touch.py / button_size_y;
    int round_x = touch.px / button_size_x;

    N3dsTouchType touch_type = button_map[round_y][round_x];
    switch (touch_type) {
    case (N3dsTouchType::GAMEPAD):
        message = std::make_shared<TouchStateChangedMsg>(touch_type);
        break;
    case (N3dsTouchType::MOUSEPAD):
        message = std::make_shared<TouchStateChangedMsg>(touch_type);
        break;
    case (N3dsTouchType::KEYBOARD):
        message = std::make_shared<TouchStateChangedMsg>(touch_type);
        break;
    case (N3dsTouchType::DISABLED):
        // Signal to exit the stream
        message = std::make_shared<GenericEventMsg>(MessageType::EXIT_STREAM);
        break;
    default:
        message = std::make_shared<TouchStateChangedMsg>(touch_type);
        break;
    }
}
