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
#include "../../system/dispatcher.hpp"

N3dsTouchscreenInput::N3dsTouchscreenInput(GAMEPAD_STATE *gamepad_in)
    : gamepad_state(gamepad_in), lock(ThreadLock::CreateLock()) {
    ThreadLock(lock.get());
    MessageDispatcher::get_instance()->subscribe(
        MessageType::TOUCH_STATE_CHANGED, this);
};

N3dsTouchscreenInput::~N3dsTouchscreenInput() {
    ThreadLock(lock.get());
    handler = nullptr;
    MessageDispatcher::get_instance()->unsubscribe(
        MessageType::TOUCH_STATE_CHANGED, this);
}

void N3dsTouchscreenInput::accept(IMessage *msg) {
    ThreadLock(lock.get());
    if (msg->getMessageType() != MessageType::TOUCH_STATE_CHANGED) {
        return;
    }

    auto touch_msg = static_cast<TouchStateChangedMsg *>(msg);
    n3dsinput_set_touch(touch_msg->ttype);
}

void N3dsTouchscreenInput::n3dsinput_set_touch(N3dsTouchType touch_type) {
    ThreadLock(lock.get());
    switch (touch_type) {
    case GAMEPAD:
        handler = std::make_unique<GamepadTouchHandler>(gamepad_state);
        break;
    case MOUSEPAD:
        handler = std::make_unique<MouseTouchHandler>();
        break;
    case KEYBOARD:
        handler = std::make_unique<KeyboardTouchHandler>();
        break;
    case ABSOLUTE_TOUCH:
        handler = std::make_unique<AbsoluteTouchHandler>(0, 1);
        break;
    case DS_TOUCH:
        // Not working? Shows up as mirror?
        handler = std::make_unique<AbsoluteTouchHandler>(GSP_SCREEN_WIDTH, 2);
        break;
    case MAGNIFY_TOUCH:
        // Causes crash, even when not accessed though the menu
        handler = std::make_unique<MagnifyTouchHandler>();
        break;
    case MENU_TOUCH:
        handler = std::make_unique<MenuTouchHandler>();
        break;
    case DEBUG_TOUCH:
        handler = std::make_unique<DebugTouchHandler>();
        break;
    default:
        handler = nullptr;
        break;
    }
}

void N3dsTouchscreenInput::n3dsinput_handle_touch(u32 kDown, u32 kUp) {
    ThreadLock(lock.get());
    if (handler == nullptr) {
        return;
    }

    touchPosition touch;
    hidTouchRead(&touch);
    if (kDown & KEY_TOUCH) {
        handler->handle_touch_down(touch);
    } else if (kUp & KEY_TOUCH) {
        handler->handle_touch_up(touch);
    } else {
        handler->handle_touch_hold(touch);
    }
}
