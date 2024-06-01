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

N3dsRendererDualScreenMirror::N3dsRendererDualScreenMirror(
    int dest_width, int dest_height, int src_width, int src_height, int px_size)
    : N3dsRendererBase(GFX_TOP, dest_width, dest_height, src_width, src_height,
                       px_size),
      top_renderer(dest_width, dest_height, src_width, src_height, px_size),
      bottom_renderer(src_width, src_height, px_size) {}

N3dsRendererDualScreenMirror::~N3dsRendererDualScreenMirror() = default;

void N3dsRendererDualScreenMirror::write_px_to_framebuffer(uint8_t *source) {
    top_renderer.write_px_to_framebuffer(source);
    bottom_renderer.write_px_to_framebuffer(source);
}
