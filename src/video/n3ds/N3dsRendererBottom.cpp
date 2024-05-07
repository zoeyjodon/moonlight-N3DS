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

inline int N3dsRendererBottom::get_dest_offset(int x, int y, int dest_height) {
    return dest_height - y - 1 + dest_height * x;
}

inline int N3dsRendererBottom::get_source_offset(int x, int y, int src_width,
                                                  int src_height,
                                                  int dest_width,
                                                  int dest_height) {
    return (x * src_width / dest_width) +
           (y * src_height / dest_height) * src_width;
}

N3dsRendererBottom::N3dsRendererBottom(int src_width, int src_height,
                                         int px_size)
    : IN3dsRenderer(GSP_SCREEN_HEIGHT_BOTTOM) {
    // Generate LUTs so we don't have to calculate pixel rotation while
    // streaming.
    offset_lut_size = GSP_SCREEN_HEIGHT_BOTTOM * GSP_SCREEN_WIDTH;
    src_offset_lut = (int *)malloc(sizeof(int) * offset_lut_size);
    if (!src_offset_lut) {
        throw std::runtime_error("Out of memory!\n");
    }
    dest_offset_lut = (int *)malloc(sizeof(int) * offset_lut_size);
    if (!dest_offset_lut) {
        throw std::runtime_error("Out of memory!\n");
    }

    int i = 0;
    for (int y = 0; y < GSP_SCREEN_WIDTH; ++y) {
        for (int x = 0; x < GSP_SCREEN_HEIGHT_BOTTOM; ++x) {
            src_offset_lut[i] =
                px_size * get_source_offset(x, y, src_width, src_height,
                                            GSP_SCREEN_HEIGHT_BOTTOM, GSP_SCREEN_WIDTH);
            dest_offset_lut[i] = px_size * get_dest_offset(x, y, GSP_SCREEN_WIDTH);
            i++;
        }
    }
}

N3dsRendererBottom::~N3dsRendererBottom() {
    if (src_offset_lut)
        free(src_offset_lut);
    if (dest_offset_lut)
        free(dest_offset_lut);
}

void N3dsRendererBottom::write_px_to_framebuffer(uint8_t *source,
                                                  int px_size) {
    u8 *dest = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    for (int i = 0; i < offset_lut_size; i++) {
        memcpy(dest + dest_offset_lut[i], source + src_offset_lut[i], px_size);
    }
    gfxScreenSwapBuffers(GFX_BOTTOM, false);
}
