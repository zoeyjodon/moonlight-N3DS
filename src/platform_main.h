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
#ifndef PLATFORM_MAIN_H
#define PLATFORM_MAIN_H

#include <Limelight.h>

#ifndef __3DS__
#include <dlfcn.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#define IS_EMBEDDED(SYSTEM) SYSTEM != SDL

enum platform { NONE, SDL, X11, X11_VDPAU, X11_VAAPI, PI, MMAL, IMX, AML, RK, FAKE };
enum codecs { CODEC_UNSPECIFIED, CODEC_H264, CODEC_HEVC, CODEC_AV1 };

enum platform platform_check(char*);
PDECODER_RENDERER_CALLBACKS platform_get_video(enum platform system);
PAUDIO_RENDERER_CALLBACKS platform_get_audio(enum platform system, char* audio_device);
bool platform_prefers_codec(enum platform system, enum codecs codec);
char* platform_name(enum platform system);

void platform_start(enum platform system);
void platform_stop(enum platform system);

#endif
