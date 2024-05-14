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
#include "keyboard_alt_bgr.h"
#include "keyboard_bgr.h"
#include "keyboard_caps_bgr.h"
#include "keyboard_shift_bgr.h"
#include <Limelight.h>
#include <cstring>

KeyboardTouchHandler::KeyboardTouchHandler()
    : selected_keycodes(&default_keycodes) {
    handle_default();
}

void KeyboardTouchHandler::set_screen(const uint8_t *bgr_buffer, int bgr_size) {
    u8 *gfxbtmadr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    memcpy(gfxbtmadr, bgr_buffer, bgr_size);

    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

void KeyboardTouchHandler::reset_shift_state() {
    shift_active = false;
    caps_active = false;
}

void KeyboardTouchHandler::handle_default() {
    set_screen(keyboard_bgr, keyboard_bgr_size);
    selected_keycodes = &default_keycodes;
    reset_shift_state();
}

void KeyboardTouchHandler::handle_shift() {
    if (caps_active) {
        handle_default();
    } else if (shift_active) {
        handle_caps();
    } else {
        set_screen(keyboard_shift_bgr, keyboard_shift_bgr_size);
        shift_active = true;
    }
}

void KeyboardTouchHandler::handle_caps() {
    set_screen(keyboard_caps_bgr, keyboard_caps_bgr_size);
    reset_shift_state();
    caps_active = true;
}

void KeyboardTouchHandler::handle_alt() {
    if (alt_active) {
        handle_default();
        alt_active = false;
    } else {
        set_screen(keyboard_alt_bgr, keyboard_alt_bgr_size);
        selected_keycodes = &alt_keycodes;
        reset_shift_state();
        alt_active = true;
    }
}

keycode_info KeyboardTouchHandler::get_keycode(touchPosition touch) {
    for (row_boundary &r_bound : key_boundaries) {
        if (touch.py > r_bound.max_y) {
            continue;
        }
        for (key_boundary &k_bound : r_bound.keys) {
            if (touch.px > k_bound.max_x) {
                continue;
            }
            if (k_bound.key_idx >= 0) {
                return (*selected_keycodes)[k_bound.key_idx];
            }
            break;
        }
        break;
    }
    return {-1, false};
}

void KeyboardTouchHandler::_handle_touch_down(touchPosition touch) {
    active_keycode = get_keycode(touch);
    if (active_keycode.code == KEYBOARD_SWITCH_KC) {
        handle_alt();
    } else if (active_keycode.code > KEYBOARD_SWITCH_KC) {
        if (active_keycode.code == SHIFT_KC) {
            handle_shift();
            active_keycode = {-1, false};
        } else {
            int modifiers =
                (shift_active || caps_active || active_keycode.require_shift)
                    ? MODIFIER_SHIFT
                    : 0;
            LiSendKeyboardEvent(active_keycode.code, KEY_ACTION_DOWN,
                                modifiers);
        }
    }
}

void KeyboardTouchHandler::_handle_touch_up(touchPosition touch) {
    if (active_keycode.code <= KEYBOARD_SWITCH_KC) {
        return;
    }

    int modifiers =
        (shift_active || caps_active || active_keycode.require_shift)
            ? MODIFIER_SHIFT
            : 0;
    LiSendKeyboardEvent(active_keycode.code, KEY_ACTION_UP, modifiers);
    if (shift_active) {
        handle_default();
    }
    active_keycode = {-1, false};
}

void KeyboardTouchHandler::_handle_touch_hold(touchPosition touch) {}
