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

N3dsTouchscreenInput::N3dsTouchscreenInput(GAMEPAD_STATE *gamepad_in,
                                           N3dsTouchType touch_type_in)
    : gamepad_state(gamepad_in), touch_type(touch_type_in) {
    init_touch_handler();
};

inline bool N3dsTouchscreenInput::change_touchpad_pressed(touchPosition touch) {
    if (touch_type == DS_TOUCH) {
        return false;
    }
    return touch.py >= 205 && touch.px >= 285;
}

inline void N3dsTouchscreenInput::init_touch_handler() {
    switch (touch_type) {
    case GAMEPAD:
        handler = std::make_unique<GamepadTouchHandler>(gamepad_state);
        break;
    case MOUSEPAD:
        handler = std::make_unique<MouseTouchHandler>();
        break;
    case ABSOLUTE_TOUCH:
        handler = std::make_unique<AbsoluteTouchHandler>(0, 1);
        break;
    case DS_TOUCH:
        handler = std::make_unique<AbsoluteTouchHandler>(GSP_SCREEN_WIDTH, 2);
        break;
    default:
        handler = nullptr;
        break;
    }
}

inline void N3dsTouchscreenInput::n3dsinput_cycle_touch() {
    switch (touch_type) {
    case GAMEPAD:
        touch_type = MOUSEPAD;
        init_touch_handler();
        break;
    case MOUSEPAD:
        touch_type = GAMEPAD;
        init_touch_handler();
        break;
    }
}

void N3dsTouchscreenInput::n3dsinput_handle_touch(u32 kDown, u32 kUp) {
    if (!handler) {
        return;
    }

    touchPosition touch;
    hidTouchRead(&touch);
    if (kDown & KEY_TOUCH) {
        if (change_touchpad_pressed(touch)) {
            n3dsinput_cycle_touch();
        } else {
            handler->handle_touch_down(touch);
        }
    } else if (kUp & KEY_TOUCH) {
        handler->handle_touch_up(touch);
    } else {
        handler->handle_touch_hold(touch);
    }
}
