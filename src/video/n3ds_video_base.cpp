/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
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

#include "../system/dispatcher.hpp"
#include "gamepad_bgr.h"
#include "keyboard_bgr.h"
#include "menu_bgr.h"
#include "touchpad_bgr.h"
#include "video.hpp"

#include <3ds.h>
#include <stdio.h>

VideoDecoderBase::VideoDecoderBase(int width, int height) {
    surface_height = GSP_SCREEN_WIDTH;
    surface_width = width > GSP_SCREEN_HEIGHT_TOP ? GSP_SCREEN_HEIGHT_TOP_2X
                                                  : GSP_SCREEN_HEIGHT_TOP;
    // Clamp output image to max dimensions supported by the renderer
    image_width = width > MOON_CTR_VIDEO_TEX_W ? MOON_CTR_VIDEO_TEX_W : width;
    image_height =
        height > MOON_CTR_VIDEO_TEX_H ? MOON_CTR_VIDEO_TEX_H : height;

    GSPGPU_FramebufferFormat px_fmt = gfxGetScreenFormat(GFX_TOP);
    pixel_size = gspGetBytesPerPixel(px_fmt);

    renderer = std::make_unique<N3dsRendererMock>();

    auto pDispatcher = MessageDispatcher::get_instance();
    pDispatcher->subscribe(MessageType::TOUCH_STATE_CHANGED, this);
    pDispatcher->subscribe(MessageType::KEYBOARD_STATE_CHANGED, this);
    pDispatcher->subscribe(MessageType::EXIT_STREAM, this);
}

VideoDecoderBase::~VideoDecoderBase() {
    auto pDispatcher = MessageDispatcher::get_instance();
    pDispatcher->unsubscribe(MessageType::TOUCH_STATE_CHANGED, this);
    pDispatcher->unsubscribe(MessageType::KEYBOARD_STATE_CHANGED, this);
    pDispatcher->unsubscribe(MessageType::EXIT_STREAM, this);
}

void VideoDecoderBase::accept(IMessage *msg) {
    switch (msg->getMessageType()) {
    case MessageType::TOUCH_STATE_CHANGED: {
        auto touch_msg = static_cast<TouchStateChangedMsg *>(msg);
        _accept_touch_state_changed(touch_msg->ttype);
        break;
    }
    case MessageType::KEYBOARD_STATE_CHANGED:
        _accept_keyboard_state_changed(
            static_cast<KeyboardStateChangedMsg *>(msg));
        break;
    case MessageType::EXIT_STREAM: {
        _accept_touch_state_changed(N3dsTouchType::DISABLED);
        printf("Exiting stream...\n");
    } break;
    default:
        break;
    }
}

void VideoDecoderBase::_accept_touch_state_changed(N3dsTouchType ttype) {
    renderer_lock.lock();
    switch (ttype) {
    case (N3dsTouchType::DEBUG_TOUCH):
        renderer = std::make_unique<N3dsRendererNormal>(
            surface_width, surface_height, image_width, image_height,
            pixel_size, true);
        break;
    case (N3dsTouchType::GAMEPAD):
        renderer = std::make_unique<N3dsRendererNormal>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        (static_cast<N3dsRendererNormal *>(renderer.get()))
            ->set_bottom_screen(gamepad_bgr);
        break;
    case (N3dsTouchType::MOUSEPAD):
        renderer = std::make_unique<N3dsRendererNormal>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        (static_cast<N3dsRendererNormal *>(renderer.get()))
            ->set_bottom_screen(touchpad_bgr);
        break;
    case (N3dsTouchType::KEYBOARD):
        renderer = std::make_unique<N3dsRendererNormal>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        (static_cast<N3dsRendererNormal *>(renderer.get()))
            ->set_bottom_screen(keyboard_bgr);
        break;
    case (N3dsTouchType::ABSOLUTE_TOUCH):
        renderer = std::make_unique<N3dsRendererDualScreenMirror>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        break;
    case (N3dsTouchType::DS_TOUCH):
        renderer = std::make_unique<N3dsRendererDualScreenStretch>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        break;
    case (N3dsTouchType::MAGNIFY_TOUCH):
        renderer = std::make_unique<N3dsRendererDualScreenMagnify>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        break;
    case (N3dsTouchType::MENU_TOUCH):
        renderer = std::make_unique<N3dsRendererNormal>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        (static_cast<N3dsRendererNormal *>(renderer.get()))
            ->set_bottom_screen(menu_bgr);
        break;
    default: // Disabled
        renderer = std::make_unique<N3dsRendererMock>();
        break;
    }
    renderer_lock.unlock();
}

void VideoDecoderBase::_accept_keyboard_state_changed(
    KeyboardStateChangedMsg *msg) {
    renderer_lock.lock();
    (static_cast<N3dsRendererNormal *>(renderer.get()))
        ->set_bottom_screen(msg->keyboard_image, msg->key_offset,
                            msg->key_size);
    renderer_lock.unlock();
}
