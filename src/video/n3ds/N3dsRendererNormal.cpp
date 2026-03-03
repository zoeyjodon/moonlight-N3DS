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

#include "N3dsRenderer.hpp"

#include <cstdlib>
#include <cstring>
#include <stdbool.h>
#include <stdexcept>
#include <unistd.h>

N3dsRendererNormal::N3dsRendererNormal(int dest_width, int dest_height,
                                       int src_width, int src_height,
                                       int px_size)
    : top_renderer(dest_width, dest_height, src_width, src_height, px_size),
      bottom_renderer(src_width, src_height, px_size) {}

N3dsRendererNormal::~N3dsRendererNormal() = default;

void N3dsRendererNormal::set_bottom_screen(const uint8_t *source, int offset,
                                           int size) {
    // TODO: Use the actual bottom renderer instead of copying to the
    // framebuffer directly
    u8 *gfxbtmadr = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

    if (size == 0) {
        size = GSP_SCREEN_HEIGHT_BOTTOM * GSP_SCREEN_WIDTH *
               bottom_renderer.get_px_size();
    }
    memcpy(gfxbtmadr + offset, source, size);

    gfxFlushBuffers();
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}

void N3dsRendererNormal::write_px_to_framebuffer(uint8_t *source) {
    top_renderer.write_px_to_framebuffer(source);
}

void N3dsRendererNormal::set_perf_decode_ticks(u64 ticks) {
    top_renderer.set_perf_decode_ticks(ticks);
}
