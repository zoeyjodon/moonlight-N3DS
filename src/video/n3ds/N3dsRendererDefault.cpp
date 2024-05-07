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

inline int N3dsRendererDefault::get_dest_offset(int x, int y, int dest_height) {
    return dest_height - y - 1 + dest_height * x;
}

inline int N3dsRendererDefault::get_source_offset(int x, int y, int src_width,
                                                  int src_height,
                                                  int dest_width,
                                                  int dest_height) {
    return (x * src_width / dest_width) +
           (y * src_height / dest_height) * src_width;
}

inline int N3dsRendererDefault::get_source_offset_3d_l(int x, int y,
                                                       int src_width,
                                                       int src_height,
                                                       int dest_width,
                                                       int dest_height) {
    return (x * (src_width / 2) / dest_width) +
           (y * src_height / dest_height) * src_width;
}

inline int N3dsRendererDefault::get_source_offset_3d_r(int x, int y,
                                                       int src_width,
                                                       int src_height,
                                                       int dest_width,
                                                       int dest_height) {
    return ((x * (src_width / 2) / dest_width) + (src_width / 2)) +
           (y * src_height / dest_height) * src_width;
}

inline void N3dsRendererDefault::init_px_to_framebuffer_2d(int dest_width,
                                                           int dest_height,
                                                           int src_width,
                                                           int src_height,
                                                           int px_size) {
    // Generate LUTs so we don't have to calculate pixel rotation while
    // streaming.
    offset_lut_size = dest_width * dest_height;
    src_offset_lut = (int *)malloc(sizeof(int) * offset_lut_size);
    if (!src_offset_lut) {
        throw std::runtime_error("Out of memory!\n");
    }
    dest_offset_lut = (int *)malloc(sizeof(int) * offset_lut_size);
    if (!dest_offset_lut) {
        throw std::runtime_error("Out of memory!\n");
    }

    int i = 0;
    for (int y = 0; y < dest_height; ++y) {
        for (int x = 0; x < dest_width; ++x) {
            src_offset_lut[i] =
                px_size * get_source_offset(x, y, src_width, src_height,
                                            dest_width, dest_height);
            dest_offset_lut[i] = px_size * get_dest_offset(x, y, dest_height);
            i++;
        }
    }
}

inline void N3dsRendererDefault::init_px_to_framebuffer_3d(int dest_width,
                                                           int dest_height,
                                                           int src_width,
                                                           int src_height,
                                                           int px_size) {
    // Generate LUTs so we don't have to calculate pixel rotation while
    // streaming.
    offset_lut_size_3d = dest_width * dest_height;
    src_offset_lut_3d_l = (int *)malloc(sizeof(int) * offset_lut_size_3d);
    if (!src_offset_lut_3d_l) {
        throw std::runtime_error("Out of memory!\n");
    }
    src_offset_lut_3d_r = (int *)malloc(sizeof(int) * offset_lut_size_3d);
    if (!src_offset_lut_3d_r) {
        throw std::runtime_error("Out of memory!\n");
    }
    dest_offset_lut_3d = (int *)malloc(sizeof(int) * offset_lut_size_3d);
    if (!dest_offset_lut_3d) {
        throw std::runtime_error("Out of memory!\n");
    }

    int i = 0;
    for (int y = 0; y < dest_height; ++y) {
        for (int x = 0; x < dest_width; ++x) {
            src_offset_lut_3d_l[i] =
                px_size * get_source_offset_3d_l(x, y, src_width, src_height,
                                                 dest_width, dest_height);
            src_offset_lut_3d_r[i] =
                px_size * get_source_offset_3d_r(x, y, src_width, src_height,
                                                 dest_width, dest_height);
            dest_offset_lut_3d[i] =
                px_size * get_dest_offset(x, y, dest_height);
            i++;
        }
    }
}

N3dsRendererDefault::N3dsRendererDefault(int dest_width, int dest_height,
                                         int src_width, int src_height,
                                         int px_size)
    : IN3dsRenderer(dest_width) {
    init_px_to_framebuffer_2d(dest_width, dest_height, src_width, src_height,
                              px_size);
    init_px_to_framebuffer_3d(GSP_SCREEN_HEIGHT_TOP, dest_height, src_width,
                              src_height, px_size);
}

N3dsRendererDefault::~N3dsRendererDefault() {
    if (src_offset_lut)
        free(src_offset_lut);
    if (src_offset_lut_3d_l)
        free(src_offset_lut_3d_l);
    if (src_offset_lut_3d_r)
        free(src_offset_lut_3d_r);
    if (dest_offset_lut)
        free(dest_offset_lut);
    if (dest_offset_lut_3d)
        free(dest_offset_lut_3d);
}

inline void N3dsRendererDefault::write_px_to_framebuffer_2D(uint8_t *source,
                                                            int px_size) {
    u8 *dest = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    for (int i = 0; i < offset_lut_size; i++) {
        memcpy(dest + dest_offset_lut[i], source + src_offset_lut[i], px_size);
    }
    gfxScreenSwapBuffers(GFX_TOP, false);
}

inline void N3dsRendererDefault::write_px_to_framebuffer_3D(uint8_t *source,
                                                            int px_size) {
    u8 *dest = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    for (int i = 0; i < offset_lut_size_3d; i++) {
        memcpy(dest + dest_offset_lut_3d[i], source + src_offset_lut_3d_l[i],
               px_size);
    }

    dest = gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL);
    for (int i = 0; i < offset_lut_size_3d; i++) {
        memcpy(dest + dest_offset_lut_3d[i], source + src_offset_lut_3d_r[i],
               px_size);
    }
    gfxScreenSwapBuffers(GFX_TOP, true);
}

void N3dsRendererDefault::write_px_to_framebuffer(uint8_t *source,
                                                  int px_size) {
    if (osGet3DSliderState() > 0.0) {
        ensure_3d_enabled();
        write_px_to_framebuffer_3D(source, px_size);
    } else {
        ensure_3d_disabled();
        write_px_to_framebuffer_2D(source, px_size);
    }
}
