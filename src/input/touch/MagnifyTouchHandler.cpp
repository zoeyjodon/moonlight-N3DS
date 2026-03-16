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
#include <3ds.h>
#include <Limelight.h>

MagnifyTouchHandler::MagnifyTouchHandler(GAMEPAD_STATE *gamepad_state,
                                         int image_width, int image_height)
    : gamepad_state(gamepad_state), image_width(image_width),
      image_height(image_height) {
    _set_touch_offsets(GSP_SCREEN_HEIGHT_BOTTOM / 2, GSP_SCREEN_WIDTH / 2);
}

void MagnifyTouchHandler::_set_touch_offsets(int center_x, int center_y) {
    int x_center_image = (center_x * image_width) / GSP_SCREEN_HEIGHT_BOTTOM;
    int y_center_image = (center_y * image_height) / GSP_SCREEN_WIDTH;

    int y_offset_image = y_center_image - (GSP_SCREEN_WIDTH / 2);

    x_touch_offset = x_center_image - (GSP_SCREEN_HEIGHT_BOTTOM / 2);
    y_touch_offset = y_center_image - (GSP_SCREEN_WIDTH / 2);

    int max_offset_x = image_width - GSP_SCREEN_HEIGHT_BOTTOM;
    if (x_touch_offset < 0) {
        x_touch_offset = 0;
    } else if (x_touch_offset > max_offset_x) {
        x_touch_offset = max_offset_x;
    }

    int max_offset_y = image_height - GSP_SCREEN_WIDTH;
    if (y_touch_offset < 0) {
        y_touch_offset = 0;
    } else if (y_touch_offset > max_offset_y) {
        y_touch_offset = max_offset_y;
    }
}

bool MagnifyTouchHandler::_lock_view() {
    return gamepad_state->buttons & (LB_FLAG | RB_FLAG);
}

void MagnifyTouchHandler::_handle_touch_down(touchPosition touch) {
    if (_lock_view()) {
        LiSendMousePositionEvent(touch.px + x_touch_offset,
                                 touch.py + y_touch_offset, image_width,
                                 image_height);
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
        return;
    }

    // Update local vars for touch calculations
    _set_touch_offsets(touch.px, touch.py);

    // Alert the rendering system to update the view
    auto pDispatcher = MessageDispatcher::get_instance();
    auto msg = std::make_shared<TouchscreenEventMsg>(
        TouchscreenEventMsgType::DOWN, touch);
    pDispatcher->post_immediate(msg);
}

void MagnifyTouchHandler::_handle_touch_up(touchPosition touch) {
    if (_lock_view()) {
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
    }
}

void MagnifyTouchHandler::_handle_touch_hold(touchPosition touch) {
    if (_lock_view()) {
        LiSendMousePositionEvent(touch.px + x_touch_offset,
                                 touch.py + y_touch_offset, image_width,
                                 image_height);
        return;
    }

    // Update local vars for touch calculations
    _set_touch_offsets(touch.px, touch.py);

    // Alert the rendering system to update the view
    auto pDispatcher = MessageDispatcher::get_instance();
    auto msg = std::make_shared<TouchscreenEventMsg>(
        TouchscreenEventMsgType::HOLD, touch);
    pDispatcher->post_immediate(msg);
}
