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
#include "vshader_shbin.h"

#include <cstdlib>
#include <cstring>
#include <stdbool.h>
#include <stdexcept>
#include <unistd.h>

N3dsRendererBottom::N3dsRendererBottom(int src_width, int src_height,
                                       int px_size, bool debug_in)
    : N3dsRendererBase(GSP_SCREEN_HEIGHT_BOTTOM, GSP_SCREEN_WIDTH, src_width,
                       src_height, px_size, debug_in) {}

N3dsRendererBottom::~N3dsRendererBottom() {}

inline void N3dsRendererBottom::write_px_to_framebuffer(uint8_t *source) {
    u8 *dest = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    u8 *dest_debug = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    write_px_to_framebuffer_gpu(source, dest, dest_debug);
    gfxScreenSwapBuffers(GFX_BOTTOM, true);
    if (debug) {
        gfxScreenSwapBuffers(GFX_TOP, true);
    }
}
