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

#include "../system/ThreadLock.hpp"
#include "../system/subscriber.hpp"
#include "../util.h"
#include "n3ds/N3dsRenderer.hpp"
#include <3ds/types.h>

enum DecodeReturnStatus { SUCCESS, NO_FRAME_PRODUCED, ERROR };

class MvdDecoder : public ISubscriber {
  public:
    MvdDecoder(int videoFormat, int width, int height, int redrawRate,
               void *context, int drFlags);
    ~MvdDecoder();
    void accept(IMessage *msg) override;
    int submit_decode_unit(PDECODE_UNIT decodeUnit);

  private:
    void _accept_touch_state_changed(TouchStateChangedMsg *msg);
    void _accept_keyboard_state_changed(KeyboardStateChangedMsg *msg);
    DecodeReturnStatus _decode(unsigned char *indata, int inlen);

  private:
    void *nal_unit_buffer = NULL;
    size_t nal_unit_buffer_size = 0;
    MVDSTD_Config mvdstd_config;
    int image_width, image_height, surface_width, surface_height, pixel_size;
    u8 *rgb_img_buffer;
    bool first_frame = true;
    std::unique_ptr<IN3dsRenderer> renderer = nullptr;
    PLockType lock;
};

extern DECODER_RENDERER_CALLBACKS decoder_callbacks_mock;
extern DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds;
extern DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds_mvd;
