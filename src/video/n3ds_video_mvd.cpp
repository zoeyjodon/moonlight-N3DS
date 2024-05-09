/*
 * This file is part of Moonlight Embedded.
 *
 * Based on Moonlight Pc implementation
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

#include "../util.h"
#include "n3ds/N3dsRenderer.hpp"
#include "video.h"

#include <3ds.h>

#include <Limelight.h>
#include <libavcodec/avcodec.h>

#include <memory>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define N3DS_DEC_BUFF_SIZE 23

// General decoder and renderer state
static void *nal_unit_buffer;
static size_t nal_unit_buffer_size;
static MVDSTD_Config mvdstd_config;

static int image_width, image_height, surface_width, surface_height, pixel_size;
static u8 *rgb_img_buffer;
static bool first_frame = true;

static std::unique_ptr<N3dsRendererBase> renderer = nullptr;

static int n3ds_init(int videoFormat, int width, int height, int redrawRate,
                     void *context, int drFlags) {
    bool is_new_3ds;
    APT_CheckNew3DS(&is_new_3ds);
    if (!is_new_3ds) {
        fprintf(stderr, "Hardware decoding is only available on the New 3DS\n");
        return -1;
    }

    int status =
        mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264, MVD_OUTPUT_BGR565,
                   width * height * N3DS_DEC_BUFF_SIZE, NULL);
    if (status) {
        fprintf(stderr, "mvdstdInit failed: %d\n", status);
        mvdstdExit();
        return -1;
    }

    surface_height = GSP_SCREEN_WIDTH;
    if (width > GSP_SCREEN_HEIGHT_TOP) {
        surface_width = GSP_SCREEN_HEIGHT_TOP_2X;
    } else {
        surface_width = GSP_SCREEN_HEIGHT_TOP;
    }

    GSPGPU_FramebufferFormat px_fmt = gfxGetScreenFormat(GFX_TOP);
    image_width = width;
    image_height = height;
    pixel_size = gspGetBytesPerPixel(px_fmt);
    rgb_img_buffer = (u8 *)linearAlloc(MOON_CTR_VIDEO_TEX_W *
                                       MOON_CTR_VIDEO_TEX_H * pixel_size);
    if (!rgb_img_buffer) {
        fprintf(stderr, "Out of memory!\n");
        return -1;
    }

    ensure_linear_buf_size(&nal_unit_buffer, &nal_unit_buffer_size,
                           INITIAL_DECODER_BUFFER_SIZE +
                               AV_INPUT_BUFFER_PADDING_SIZE);
    mvdstdGenerateDefaultConfig(&mvdstd_config, image_width, image_height,
                                image_width, image_height, NULL,
                                (u32 *)rgb_img_buffer, NULL);

    // Place within the 1024x512 buffer
    mvdstd_config.flag_x104 = 1;
    mvdstd_config.output_width_override = MOON_CTR_VIDEO_TEX_W;
    mvdstd_config.output_height_override = MOON_CTR_VIDEO_TEX_H;
    MVDSTD_SetConfig(&mvdstd_config);

    switch (N3DS_RENDER_TYPE) {
    case (RENDER_BOTTOM):
        renderer = std::make_unique<N3dsRendererBottom>(
            image_width, image_height, pixel_size);
        break;
    case (RENDER_DUAL_SCREEN):
        renderer = std::make_unique<N3dsRendererDualScreen>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        break;
    default:
        renderer = std::make_unique<N3dsRendererDefault>(
            surface_width, surface_height, image_width, image_height,
            pixel_size);
        break;
    }
    return 0;
}

// This function must be called after
// decoding is finished
static void n3ds_destroy(void) {
    y2rExit();
    mvdstdExit();
    linearFree(nal_unit_buffer);
    linearFree(rgb_img_buffer);
    renderer = nullptr;
}

// packets must be decoded in order
// indata must be inlen + AV_INPUT_BUFFER_PADDING_SIZE in length
static inline int n3ds_decode(unsigned char *indata, int inlen) {
    int ret = mvdstdProcessVideoFrame(indata, inlen, 1, NULL);
    if (ret != MVD_STATUS_PARAMSET && ret != MVD_STATUS_INCOMPLETEPROCESSING) {
        mvdstdRenderVideoFrame(&mvdstd_config, true);
    }
    return 0;
}

static int n3ds_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    u64 start_ticks = svcGetSystemTick();
    PLENTRY entry = decodeUnit->bufferList;
    int length = 0;

    ensure_linear_buf_size(&nal_unit_buffer, &nal_unit_buffer_size,
                           decodeUnit->fullLength +
                               AV_INPUT_BUFFER_PADDING_SIZE);

    while (entry != NULL) {
        memcpy(nal_unit_buffer + length, entry->data, entry->length);
        length += entry->length;
        entry = entry->next;
    }
    GSPGPU_FlushDataCache(nal_unit_buffer, length);

    n3ds_decode((unsigned char *)nal_unit_buffer, length);
    renderer->perf_decode_ticks = svcGetSystemTick() - start_ticks;

    renderer->write_px_to_framebuffer(rgb_img_buffer, pixel_size);

    // If MVD never gets an IDR frame, everything shows up gray
    if (first_frame) {
        first_frame = false;
        return DR_NEED_IDR;
    }
    return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds_mvd = {
    .setup = n3ds_init,
    .cleanup = n3ds_destroy,
    .submitDecodeUnit = n3ds_submit_decode_unit,
    .capabilities = CAPABILITY_DIRECT_SUBMIT,
};
