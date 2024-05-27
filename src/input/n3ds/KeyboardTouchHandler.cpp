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
#include "keyboard_lock_bgr.h"
#include "keyboard_shift_bgr.h"
#include "keyboard_temp_bgr.h"
#include <Limelight.h>
#include <cstring>

const static int KEY_PX_SIZE = 3;

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

void KeyboardTouchHandler::set_screen_key(KeyInfo &key_info) {
    u8 *gfxbtmadr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    const uint8_t *bgr_buffer;
    switch (key_info.state) {
    case (KEY_TEMPORARY):
        bgr_buffer = keyboard_temp_bgr;
        break;
    case (KEY_LOCKED):
        bgr_buffer = keyboard_lock_bgr;
        break;
    case (KEY_SHIFT):
        bgr_buffer = keyboard_shift_bgr;
        break;
    default:
        bgr_buffer = alt_keyboard_active ? keyboard_alt_bgr : keyboard_bgr;
        break;
    }

    for (int x = key_info.min_x; x < key_info.max_x; x++) {
        int bgr_offset =
            ((GSP_SCREEN_WIDTH * x) + (GSP_SCREEN_WIDTH - key_info.max_y)) *
            KEY_PX_SIZE;
        int bgr_size = (key_info.max_y - key_info.min_y) * KEY_PX_SIZE;
        memcpy(gfxbtmadr + bgr_offset, bgr_buffer + bgr_offset, bgr_size);
    }

    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

void KeyboardTouchHandler::set_shift_keys() {
    shift_keys.state =
        !alt_keyboard_active && (shift_info.state != KEY_DISABLED)
            ? KEY_SHIFT
            : KEY_DISABLED;
    set_screen_key(shift_keys);
}

void KeyboardTouchHandler::handle_default() {
    set_screen(keyboard_bgr, keyboard_bgr_size);
    selected_keycodes = &default_keycodes;
    alt_keyboard_active = false;
    set_screen_key(shift_info);
    set_screen_key(ctrl_info);
    set_screen_key(alt_info);
    set_shift_keys();
}

void KeyboardTouchHandler::cycle_key_state(KeyInfo &key_info) {
    switch (key_info.state) {
    case (KEY_TEMPORARY):
        key_info.state = KEY_LOCKED;
        break;
    case (KEY_LOCKED):
        key_info.state = KEY_DISABLED;
        break;
    default:
        key_info.state = KEY_TEMPORARY;
        break;
    }
    set_screen_key(key_info);
}

void KeyboardTouchHandler::handle_alt_keyboard() {
    if (alt_keyboard_active) {
        handle_default();
    } else {
        set_screen(keyboard_alt_bgr, keyboard_alt_bgr_size);
        selected_keycodes = &alt_keycodes;
        alt_keyboard_active = true;
        set_screen_key(shift_info);
        set_screen_key(ctrl_info);
        set_screen_key(alt_info);
        set_shift_keys();
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

int KeyboardTouchHandler::get_key_mod() {
    int modifiers = 0;
    modifiers |=
        (shift_info.state != KEY_DISABLED || active_keycode.require_shift)
            ? MODIFIER_SHIFT
            : 0;
    modifiers |= (ctrl_info.state != KEY_DISABLED) ? MODIFIER_CTRL : 0;
    modifiers |= (alt_info.state != KEY_DISABLED) ? MODIFIER_ALT : 0;
    return modifiers;
}

void KeyboardTouchHandler::_handle_touch_down(touchPosition touch) {
    active_keycode = get_keycode(touch);
    if (active_keycode.code == KEYBOARD_SWITCH_KC) {
        handle_alt_keyboard();
    } else if (active_keycode.code > KEYBOARD_SWITCH_KC) {
        if (active_keycode.code == SHIFT_KC) {
            cycle_key_state(shift_info);
            set_shift_keys();
        } else if (active_keycode.code == CTRL_KC) {
            cycle_key_state(ctrl_info);
        } else if (active_keycode.code == ALT_KC) {
            cycle_key_state(alt_info);
        }
        int modifiers = get_key_mod();
        LiSendKeyboardEvent(active_keycode.code, KEY_ACTION_DOWN, modifiers);
    }
}

void KeyboardTouchHandler::_handle_touch_up(touchPosition touch) {
    if (active_keycode.code <= KEYBOARD_SWITCH_KC) {
        return;
    }

    int modifiers = get_key_mod();
    LiSendKeyboardEvent(active_keycode.code, KEY_ACTION_UP, modifiers);

    if (active_keycode.code != SHIFT_KC && active_keycode.code != CTRL_KC &&
        active_keycode.code != ALT_KC) {
        if (shift_info.state == KEY_TEMPORARY) {
            shift_info.state = KEY_DISABLED;
            set_screen_key(shift_info);
            set_shift_keys();
        }
        if (ctrl_info.state == KEY_TEMPORARY) {
            ctrl_info.state = KEY_DISABLED;
            set_screen_key(ctrl_info);
        }
        if (alt_info.state == KEY_TEMPORARY) {
            alt_info.state = KEY_DISABLED;
            set_screen_key(alt_info);
        }
    }
    active_keycode = {-1, false};
}

void KeyboardTouchHandler::_handle_touch_hold(touchPosition touch) {}
