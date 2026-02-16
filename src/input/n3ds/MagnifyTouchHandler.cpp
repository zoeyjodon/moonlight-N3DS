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

void MagnifyTouchHandler::_handle_touch_down(touchPosition touch) {
    if (magnify_renderer_instance) {
        magnify_renderer_instance->set_crop_region(touch.px, touch.py);
    }
}

void MagnifyTouchHandler::_handle_touch_up(touchPosition touch) {}

void MagnifyTouchHandler::_handle_touch_hold(touchPosition touch) {
    if (magnify_renderer_instance) {
        magnify_renderer_instance->set_crop_region(touch.px, touch.py);
    }
}
