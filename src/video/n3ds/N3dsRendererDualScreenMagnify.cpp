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

#include "../../system/dispatcher.hpp"
#include "N3dsRenderer.hpp"

#include <cstdlib>
#include <cstring>
#include <stdbool.h>
#include <stdexcept>
#include <unistd.h>

N3dsRendererDualScreenMagnify::N3dsRendererDualScreenMagnify(
    int dest_width, int dest_height, int src_width, int src_height, int px_size)
    : image_width(src_width), image_height(src_height), px_size(px_size),
      top_renderer(dest_width, dest_height, src_width, src_height, px_size),
      bottom_renderer(GSP_SCREEN_HEIGHT_BOTTOM, GSP_SCREEN_WIDTH, px_size) {
    set_crop_region(GSP_SCREEN_HEIGHT_BOTTOM / 2, GSP_SCREEN_WIDTH / 2);

    auto pDispatcher = MessageDispatcher::get_instance();
    pDispatcher->subscribe(MessageType::TOUCHSCREEN_EVENT, this);
}

N3dsRendererDualScreenMagnify::~N3dsRendererDualScreenMagnify() {
    auto pDispatcher = MessageDispatcher::get_instance();
    pDispatcher->unsubscribe(MessageType::TOUCHSCREEN_EVENT, this);
}

void N3dsRendererDualScreenMagnify::accept(IMessage *msg) {
    if (msg->getMessageType() != MessageType::TOUCHSCREEN_EVENT) {
        return;
    }
    auto touch_msg = static_cast<TouchscreenEventMsg *>(msg);
    set_crop_region(touch_msg->touch.px, touch_msg->touch.py);
}

void N3dsRendererDualScreenMagnify::set_crop_region(int center_x,
                                                    int center_y) {
    int crop_width = GSP_SCREEN_HEIGHT_BOTTOM;
    int crop_height = GSP_SCREEN_WIDTH;

    int crop_offset_x =
        ((image_width - crop_width) * center_x) / GSP_SCREEN_HEIGHT_BOTTOM;

    if (crop_offset_x < 0)
        crop_offset_x = 0;
    else if (crop_offset_x + crop_width > image_width)
        crop_offset_x = image_width - crop_width;

    int crop_offset_y =
        ((image_height - crop_height) * center_y) / GSP_SCREEN_WIDTH;
    if (crop_offset_y < 0)
        crop_offset_y = 0;
    else if (crop_offset_y + crop_height > image_height)
        crop_offset_y = image_height - crop_height;

    int line_stride = MOON_CTR_VIDEO_TEX_W * px_size;

    pixel_offset = crop_offset_y * line_stride + crop_offset_x * px_size;
}

void N3dsRendererDualScreenMagnify::write_px_to_framebuffer(uint8_t *source) {
    // Render full resolution on top screen
    top_renderer.write_px_to_framebuffer(source);
    // Render magnified region on bottom screen
    bottom_renderer.write_px_to_framebuffer(source + pixel_offset);
}

void N3dsRendererDualScreenMagnify::set_perf_decode_ticks(u64 ticks) {
    top_renderer.set_perf_decode_ticks(ticks);
}
