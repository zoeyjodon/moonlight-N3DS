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
#include "touchpad_bgr.h"
#include <Limelight.h>
#include <cstring>

#define N3DS_MOUSEPAD_SENSITIVITY 3

MouseTouchHandler::MouseTouchHandler() {
    u8 *gfxbtmadr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    memcpy(gfxbtmadr, touchpad_bgr, touchpad_bgr_size);

    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

void MouseTouchHandler::_handle_touch_down(touchPosition touch) {
    if (touch.py < 175) {
        previous_x = touch.px;
        previous_y = touch.py;
        if (touch.px > 285) {
            v_scroll = true;
        } else if (touch.py < 35) {
            h_scroll = true;
        }
    } else if (touch.px > 160) {
        mouse_button = BUTTON_RIGHT;
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_RIGHT);
    } else {
        mouse_button = BUTTON_LEFT;
        LiSendMouseButtonEvent(BUTTON_ACTION_PRESS, BUTTON_LEFT);
    }
}

void MouseTouchHandler::_handle_touch_up(touchPosition touch) {
    if (mouse_button > -1) {
        LiSendMouseButtonEvent(BUTTON_ACTION_RELEASE, mouse_button);
    }
    mouse_button = -1;
    previous_x = -1;
    previous_y = -1;
    v_scroll = false;
    h_scroll = false;
}

void MouseTouchHandler::_handle_touch_hold(touchPosition touch) {
    if (previous_x == -1) {
        return;
    }
    short deltaX = touch.px - previous_x;
    short deltaY = touch.py - previous_y;
    previous_x = touch.px;
    previous_y = touch.py;

    if (v_scroll) {
        LiSendScrollEvent(-1 * deltaY);
    } else if (h_scroll) {
        LiSendHScrollEvent(deltaX);
    } else {
        LiSendMouseMoveEvent(N3DS_MOUSEPAD_SENSITIVITY * deltaX,
                             N3DS_MOUSEPAD_SENSITIVITY * deltaY);
    }
}
