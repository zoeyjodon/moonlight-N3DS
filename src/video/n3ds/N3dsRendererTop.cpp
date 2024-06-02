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

#include <3ds.h>
#include <cstdlib>
#include <cstring>
#include <stdbool.h>
#include <stdexcept>
#include <unistd.h>

N3dsRendererTop::N3dsRendererTop(int dest_width, int dest_height, int src_width,
                                 int src_height, int px_size, bool debug_in)
    : N3dsRendererBase(GFX_TOP, dest_width, dest_height, src_width, src_height,
                       px_size, debug_in) {}

N3dsRendererTop::~N3dsRendererTop() = default;

void N3dsRendererTop::write_px_to_framebuffer(uint8_t *source) {
    // TODO: Add logic for stretching 400px images to fit 2 400px screen buffers
    if (osGet3DSliderState() > 0.0 &&
        surface_width >= GSP_SCREEN_HEIGHT_TOP_2X) {
        ensure_3d_enabled();
    } else {
        ensure_3d_disabled();
    }
    write_px_to_framebuffer_gpu(source);
}
