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

#include "ffmpeg.h"
#include "video.h"

#include "../util.h"

#include <3ds.h>
#include <stdbool.h>
#include <unistd.h>

#define SLICES_PER_FRAME 1
#define N3DS_BUFFER_FRAMES 1

static void *ffmpeg_buffer;
static size_t ffmpeg_buffer_size;
static int image_width, image_height, surface_width, surface_height, pixel_size;
static u8 *img_buffer;

static int offset_lut_size;
static int *dest_offset_lut;
static int *src_offset_lut;

static int offset_lut_size_3d;
static int *dest_offset_lut_3d;
static int *src_offset_lut_3d_l;
static int *src_offset_lut_3d_r;

static inline int get_dest_offset(int x, int y, int dest_height) {
    return dest_height - y - 1 + dest_height * x;
}

static inline int get_source_offset(int x, int y, int src_width, int src_height,
                                    int dest_width, int dest_height) {
    return (x * src_width / dest_width) +
           (y * src_height / dest_height) * src_width;
}

static inline int get_source_offset_3d_l(int x, int y, int src_width,
                                         int src_height, int dest_width,
                                         int dest_height) {
    return (x * (src_width / 2) / dest_width) +
           (y * src_height / dest_height) * src_width;
}

static inline int get_source_offset_3d_r(int x, int y, int src_width,
                                         int src_height, int dest_width,
                                         int dest_height) {
    return ((x * (src_width / 2) / dest_width) + (src_width / 2)) +
           (y * src_height / dest_height) * src_width;
}

static int n3ds_setup(int videoFormat, int width, int height, int redrawRate,
                      void *context, int drFlags) {
    if (ffmpeg_init(videoFormat, width, height, 0, N3DS_BUFFER_FRAMES,
                    SLICES_PER_FRAME) < 0) {
        fprintf(stderr, "Couldn't initialize video decoding\n");
        return -1;
    }

    ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size,
                    INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);

    if (y2rInit()) {
        fprintf(stderr, "Failed to initialize Y2R\n");
        return -1;
    }
    Y2RU_ConversionParams y2r_parameters;
    y2r_parameters.input_format = INPUT_YUV420_INDIV_8;
    y2r_parameters.output_format = OUTPUT_RGB_16_565;
    y2r_parameters.rotation = ROTATION_NONE;
    y2r_parameters.block_alignment = BLOCK_LINE;
    y2r_parameters.input_line_width = width;
    y2r_parameters.input_lines = height;
    y2r_parameters.standard_coefficient = COEFFICIENT_ITU_R_BT_709_SCALING;
    y2r_parameters.alpha = 0xFF;
    int status = Y2RU_SetConversionParams(&y2r_parameters);
    if (status) {
        fprintf(stderr, "Failed to set Y2RU params\n");
        return -1;
    }

    surface_height = GSP_SCREEN_WIDTH;
    if (width > GSP_SCREEN_HEIGHT_TOP) {
        surface_width = GSP_SCREEN_HEIGHT_TOP_2X;
        gfxSet3D(false);
        gfxSetWide(true);
    } else {
        gfxSet3D(false);
        gfxSetWide(false);
        surface_width = GSP_SCREEN_HEIGHT_TOP;
    }

    GSPGPU_FramebufferFormat px_fmt = gfxGetScreenFormat(GFX_TOP);
    image_width = width;
    image_height = height;
    pixel_size = gspGetBytesPerPixel(px_fmt);

    img_buffer = linearAlloc(width * height * pixel_size);
    if (!img_buffer) {
        fprintf(stderr, "Out of memory!\n");
        return -1;
    }

    return init_px_to_framebuffer(surface_width, surface_height, image_width,
                                  image_height, pixel_size);
}

static void n3ds_cleanup() {
    ffmpeg_destroy();
    y2rExit();
    linearFree(img_buffer);
    deinit_px_to_framebuffer();
}

static inline int init_px_to_framebuffer_2d(int dest_width, int dest_height,
                                            int src_width, int src_height,
                                            int px_size) {
    // Generate LUTs so we don't have to calculate pixel rotation while
    // streaming.
    offset_lut_size = dest_width * dest_height;
    src_offset_lut = malloc(sizeof(int) * offset_lut_size);
    if (!src_offset_lut) {
        fprintf(stderr, "Out of memory!\n");
        return -1;
    }
    dest_offset_lut = malloc(sizeof(int) * offset_lut_size);
    if (!dest_offset_lut) {
        fprintf(stderr, "Out of memory!\n");
        return -1;
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
    return 0;
}

static inline int init_px_to_framebuffer_3d(int dest_width, int dest_height,
                                            int src_width, int src_height,
                                            int px_size) {
    // Generate LUTs so we don't have to calculate pixel rotation while
    // streaming.
    offset_lut_size_3d = dest_width * dest_height;
    src_offset_lut_3d_l = malloc(sizeof(int) * offset_lut_size_3d);
    if (!src_offset_lut_3d_l) {
        fprintf(stderr, "Out of memory!\n");
        return -1;
    }
    src_offset_lut_3d_r = malloc(sizeof(int) * offset_lut_size_3d);
    if (!src_offset_lut_3d_r) {
        fprintf(stderr, "Out of memory!\n");
        return -1;
    }
    dest_offset_lut_3d = malloc(sizeof(int) * offset_lut_size_3d);
    if (!dest_offset_lut_3d) {
        fprintf(stderr, "Out of memory!\n");
        return -1;
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
    return 0;
}

int init_px_to_framebuffer(int dest_width, int dest_height, int src_width,
                           int src_height, int px_size) {
    surface_width = dest_width;
    surface_height = dest_height;
    int ret = init_px_to_framebuffer_2d(dest_width, dest_height, src_width,
                                        src_height, px_size);
    if (ret == 0) {
        ret = init_px_to_framebuffer_3d(GSP_SCREEN_HEIGHT_TOP, dest_height,
                                        src_width, src_height, px_size);
    }
    return ret;
}

void deinit_px_to_framebuffer() {
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

static inline void write_px_to_framebuffer_2D(uint8_t *source, int px_size) {
    u8 *dest = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    for (int i = 0; i < offset_lut_size; i++) {
        memcpy(dest + dest_offset_lut[i], source + src_offset_lut[i], px_size);
    }
    gfxScreenSwapBuffers(GFX_TOP, false);
}

static inline void write_px_to_framebuffer_3D(uint8_t *source, int px_size) {
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

void write_px_to_framebuffer(uint8_t *source, int px_size) {
    if (osGet3DSliderState() > 0.0) {
        if (!gfxIs3D()) {
            gfxSetWide(false);
            gfxSet3D(true);
        }
        write_px_to_framebuffer_3D(source, px_size);
    } else {
        if (gfxIs3D()) {
            gfxSet3D(false);
            if (surface_width == GSP_SCREEN_HEIGHT_TOP_2X) {
                gfxSetWide(true);
            }
        }
        write_px_to_framebuffer_2D(source, px_size);
    }
}

static inline int write_yuv_to_framebuffer(const u8 **source, int width,
                                           int height, int px_size) {
    Handle conversion_finish_event_handle;
    int status = 0;

    status = Y2RU_SetSendingY(source[0], width * height, width, 0);
    if (status) {
        fprintf(stderr, "Y2RU_SetSendingY failed\n");
        goto y2ru_failed;
    }

    status = Y2RU_SetSendingU(source[1], width * height / 4, width / 2, 0);
    if (status) {
        fprintf(stderr, "Y2RU_SetSendingU failed\n");
        goto y2ru_failed;
    }

    status = Y2RU_SetSendingV(source[2], width * height / 4, width / 2, 0);
    if (status) {
        fprintf(stderr, "Y2RU_SetSendingV failed\n");
        goto y2ru_failed;
    }

    status = Y2RU_SetReceiving(img_buffer, width * height * px_size, 8, 0);
    if (status) {
        fprintf(stderr, "Y2RU_SetReceiving failed\n");
        goto y2ru_failed;
    }

    status = Y2RU_StartConversion();
    if (status) {
        fprintf(stderr, "Y2RU_StartConversion failed\n");
        goto y2ru_failed;
    }

    status = Y2RU_GetTransferEndEvent(&conversion_finish_event_handle);
    if (status) {
        fprintf(stderr, "Y2RU_GetTransferEndEvent failed\n");
        goto y2ru_failed;
    }

    svcWaitSynchronization(conversion_finish_event_handle,
                           10000000); // Wait up to 10ms.
    svcCloseHandle(conversion_finish_event_handle);
    write_px_to_framebuffer(img_buffer, px_size);
    return DR_OK;

y2ru_failed:
    return -1;
}

static int n3ds_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    PLENTRY entry = decodeUnit->bufferList;
    int length = 0;

    ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size,
                    decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE);

    while (entry != NULL) {
        memcpy(ffmpeg_buffer + length, entry->data, entry->length);
        length += entry->length;
        entry = entry->next;
    }
    ffmpeg_decode(ffmpeg_buffer, length);

    AVFrame *frame = ffmpeg_get_frame(false);
    int status = write_yuv_to_framebuffer(frame->data, image_width,
                                          image_height, pixel_size);

    return status;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds = {
    .setup = n3ds_setup,
    .cleanup = n3ds_cleanup,
    .submitDecodeUnit = n3ds_submit_decode_unit,
    .capabilities =
        CAPABILITY_DIRECT_SUBMIT | CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC,
};
