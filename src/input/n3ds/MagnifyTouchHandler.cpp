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
#include "N3dsTouchscreenInput.hpp"
#include <3ds.h>

void MagnifyTouchHandler::_handle_touch_down(touchPosition touch) {
    auto pDispatcher = MessageDispatcher::get_instance();
    auto msg = TouchscreenEventMsg(TouchscreenEventMsgType::DOWN, touch);
    pDispatcher->post_immediate(&msg);
}

void MagnifyTouchHandler::_handle_touch_up(touchPosition touch) {}

void MagnifyTouchHandler::_handle_touch_hold(touchPosition touch) {
    auto pDispatcher = MessageDispatcher::get_instance();
    auto msg = TouchscreenEventMsg(TouchscreenEventMsgType::HOLD, touch);
    pDispatcher->post_immediate(&msg);
}
