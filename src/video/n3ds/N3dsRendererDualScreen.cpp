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

inline int N3dsRendererDualScreen::get_dest_offset(int x, int y,
                                                   int dest_height) {
    return dest_height - y - 1 + dest_height * x;
}

inline int N3dsRendererDualScreen::get_source_offset_ds_top(int x, int y,
                                                            int src_width,
                                                            int src_height,
                                                            int dest_width,
                                                            int dest_height) {
    return (x * src_width / dest_width) +
           (y * (src_height / 2) / dest_height) * src_width;
}

inline int N3dsRendererDualScreen::get_source_offset_ds_bottom(
    int x, int y, int src_width, int src_height, int dest_width,
    int dest_height) {
    return (x * src_width / dest_width) +
           ((y * (src_height / 2) / dest_height) + (src_height / 2)) *
               src_width;
}

inline void N3dsRendererDualScreen::init_px_to_framebuffer_ds(int dest_width,
                                                              int dest_height,
                                                              int src_width,
                                                              int src_height,
                                                              int px_size) {
    // Generate LUTs so we don't have to calculate pixel rotation while
    // streaming.
    offset_lut_size = dest_width * dest_height;
    offset_lut_size_ds_bottom = GSP_SCREEN_HEIGHT_BOTTOM * dest_height;
    src_offset_lut_ds_top = (int *)malloc(sizeof(int) * offset_lut_size);
    if (!src_offset_lut_ds_top) {
        throw std::runtime_error("Out of memory!\n");
    }
    src_offset_lut_ds_bottom =
        (int *)malloc(sizeof(int) * offset_lut_size_ds_bottom);
    if (!src_offset_lut_ds_bottom) {
        throw std::runtime_error("Out of memory!\n");
    }
    dest_offset_lut = (int *)malloc(sizeof(int) * offset_lut_size);
    if (!dest_offset_lut) {
        throw std::runtime_error("Out of memory!\n");
    }
    dest_offset_lut_ds_bottom =
        (int *)malloc(sizeof(int) * offset_lut_size_ds_bottom);
    if (!dest_offset_lut_ds_bottom) {
        throw std::runtime_error("Out of memory!\n");
    }

    int i = 0;
    for (int y = 0; y < dest_height; ++y) {
        for (int x = 0; x < dest_width; ++x) {
            src_offset_lut_ds_top[i] =
                px_size * get_source_offset_ds_top(x, y, src_width, src_height,
                                                   dest_width, dest_height);
            dest_offset_lut[i] = px_size * get_dest_offset(x, y, dest_height);
            i++;
        }
    }

    i = 0;
    for (int y = 0; y < dest_height; ++y) {
        for (int x = 0; x < GSP_SCREEN_HEIGHT_BOTTOM; ++x) {
            src_offset_lut_ds_bottom[i] =
                px_size * get_source_offset_ds_bottom(
                              x, y, src_width, src_height,
                              GSP_SCREEN_HEIGHT_BOTTOM, dest_height);
            dest_offset_lut_ds_bottom[i] =
                px_size * get_dest_offset(x, y, dest_height);
            i++;
        }
    }
}

N3dsRendererDualScreen::N3dsRendererDualScreen(int dest_width, int dest_height,
                                               int src_width, int src_height,
                                               int px_size)
    : IN3dsRenderer(dest_width) {
    ensure_3d_disabled();
    init_px_to_framebuffer_ds(dest_width, dest_height, src_width, src_height,
                              px_size);
}

N3dsRendererDualScreen::~N3dsRendererDualScreen() {
    if (src_offset_lut_ds_top)
        free(src_offset_lut_ds_top);
    if (src_offset_lut_ds_bottom)
        free(src_offset_lut_ds_bottom);
    if (dest_offset_lut)
        free(dest_offset_lut);
    if (dest_offset_lut_ds_bottom)
        free(dest_offset_lut_ds_bottom);
}

void N3dsRendererDualScreen::write_px_to_framebuffer(uint8_t *source,
                                                     int px_size) {
    u8 *dest = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    for (int i = 0; i < offset_lut_size; i++) {
        memcpy(dest + dest_offset_lut[i], source + src_offset_lut_ds_top[i],
               px_size);
    }

    dest = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    for (int i = 0; i < offset_lut_size_ds_bottom; i++) {
        memcpy(dest + dest_offset_lut_ds_bottom[i],
               source + src_offset_lut_ds_bottom[i], px_size);
    }
    gfxSwapBuffers();
}
