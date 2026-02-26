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

#pragma once

#include <Limelight.h>

#include <stdbool.h>

#define DISPLAY_FULLSCREEN 1
#define ENABLE_HARDWARE_ACCELERATION_1 2
#define ENABLE_HARDWARE_ACCELERATION_2 4
#define DISPLAY_ROTATE_MASK 24
#define DISPLAY_ROTATE_90 8
#define DISPLAY_ROTATE_180 16
#define DISPLAY_ROTATE_270 24

#define INIT_EGL 1
#define INIT_VDPAU 2
#define INIT_VAAPI 3

#define INITIAL_DECODER_BUFFER_SIZE (256 * 1024)

#include <3ds/types.h>

enum N3dsRenderType {
    RENDER_DEFAULT,
    RENDER_BOTTOM,
    RENDER_DUAL_SCREEN_STRETCH,
    RENDER_DUAL_SCREEN_MIRROR,
    RENDER_DUAL_SCREEN_MAGNIFY
};

struct VideoRendererContext {
    N3dsRenderType type;
};
extern DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds;
extern DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds_mvd;
