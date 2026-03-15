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
#include "renderer/N3dsRenderer.hpp"
#include <3ds/types.h>

enum DecodeReturnStatus { SUCCESS, NO_FRAME_PRODUCED, ERROR };

class VideoDecoderBase : public ISubscriber {
  public:
    VideoDecoderBase(int width, int height);
    virtual ~VideoDecoderBase();
    void accept(IMessage *msg) override;

  private:
    void _accept_touch_state_changed(N3dsTouchType ttype);
    void _accept_keyboard_state_changed(KeyboardStateChangedMsg *msg);

  protected:
    int image_width, image_height, surface_width, surface_height, pixel_size;
    std::unique_ptr<IN3dsRenderer> renderer = nullptr;
    ThreadLock renderer_lock;
};

class SoftVideoDecoder : public VideoDecoderBase {
  public:
    SoftVideoDecoder(int videoFormat, int width, int height, int redrawRate,
                     void *context, int drFlags);
    ~SoftVideoDecoder();
    int submit_decode_unit(PDECODE_UNIT decodeUnit);

  private:
    inline int _write_yuv_to_framebuffer(const u8 **source, int width,
                                         int height, int px_size);

  private:
    void *ffmpeg_buffer;
    size_t ffmpeg_buffer_size;
    u8 *rgb_img_buffer;
};

class MvdDecoder : public VideoDecoderBase {
  public:
    MvdDecoder(int videoFormat, int width, int height, int redrawRate,
               void *context, int drFlags);
    ~MvdDecoder();
    int submit_decode_unit(PDECODE_UNIT decodeUnit);

  private:
    DecodeReturnStatus _decode(unsigned char *indata, int inlen);

  private:
    void *nal_unit_buffer = NULL;
    size_t nal_unit_buffer_size = 0;
    MVDSTD_Config mvdstd_config;
    u8 *rgb_img_buffer;
    bool first_frame = true;
};

extern DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds;
extern DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds_mvd;
