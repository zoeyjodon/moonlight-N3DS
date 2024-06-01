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

#include "N3dsTouchscreenInput.hpp"
#include <Limelight.h>

void AbsoluteTouchHandler::_handle_touch_down(touchPosition touch) {
    LiSendMousePositionEvent(touch.px, touch.py + y_offset,
                             GSP_SCREEN_HEIGHT_BOTTOM,
                             y_scale * GSP_SCREEN_WIDTH);
    LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
}

void AbsoluteTouchHandler::_handle_touch_up(touchPosition touch) {
    LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, BUTTON_LEFT);
}

void AbsoluteTouchHandler::_handle_touch_hold(touchPosition touch) {
    LiSendMousePositionEvent(touch.px, touch.py + y_offset,
                             GSP_SCREEN_HEIGHT_BOTTOM,
                             y_scale * GSP_SCREEN_WIDTH);
}
