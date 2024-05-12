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
#include "gamepad_bgr.h"
#include <Limelight.h>
#include <cstring>

GamepadTouchHandler::GamepadTouchHandler(GAMEPAD_STATE *gamepad_in)
    : gamepad_state(gamepad_in) {
    gfxSetDoubleBuffering(GFX_BOTTOM, false);
    u8 *gfxbtmadr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    memcpy(gfxbtmadr, gamepad_bgr, gamepad_bgr_size);

    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

void GamepadTouchHandler::_handle_touch_down(touchPosition touch) {
    if (touch.py >= 120) {
        gamepad_state->buttons |= SPECIAL_FLAG;
        return;
    }

    if (touch.px < 235)
        gamepad_state->buttons |= LS_CLK_FLAG;
    if (touch.px > 104)
        gamepad_state->buttons |= RS_CLK_FLAG;
}

void GamepadTouchHandler::_handle_touch_up(touchPosition touch) {
    gamepad_state->buttons &= ~(SPECIAL_FLAG | LS_CLK_FLAG | RS_CLK_FLAG);
}

void GamepadTouchHandler::_handle_touch_hold(touchPosition touch) {}
