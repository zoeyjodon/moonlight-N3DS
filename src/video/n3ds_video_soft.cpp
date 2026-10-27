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
#include "h263_compat.h"
#if N3DS_H263_AVAILABLE
#include "h263_util.h"
#endif
#include "video.hpp"

#include "../util.h"

#include <3ds.h>
#include <memory>
#include <stdbool.h>
#include <stdexcept>
#include <unistd.h>

#define SLICES_PER_FRAME 1
#define N3DS_BUFFER_FRAMES 1

// Don't flood the console (and slow the decoder down) when a stream is bad
#define H263_LOG_LIMIT 3

static std::unique_ptr<SoftVideoDecoder> instance = nullptr;

SoftVideoDecoder::SoftVideoDecoder(int videoFormat, int width, int height,
                                   int redrawRate, void *context, int drFlags)
    : VideoDecoderBase(width, height), video_format(videoFormat) {
    printf("Video format: %s\n",
           (videoFormat & VIDEO_FORMAT_MASK_H263) ? "H.263+" : "H.264");
    if (ffmpeg_init(videoFormat, width, height, 0, N3DS_BUFFER_FRAMES,
                    SLICES_PER_FRAME) < 0) {
        fprintf(stderr, "Couldn't initialize video decoding\n");
        throw std::runtime_error("Couldn't initialize video decoding\n");
    }

    ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size,
                    INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);

    if (y2rInit()) {
        fprintf(stderr, "Failed to initialize Y2R\n");
        throw std::runtime_error("Failed to initialize Y2R\n");
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
        throw std::runtime_error("Failed to set Y2RU params\n");
    }

    rgb_img_buffer = (u8 *)linearAlloc(MOON_CTR_VIDEO_TEX_W *
                                       MOON_CTR_VIDEO_TEX_H * pixel_size);
    if (!rgb_img_buffer) {
        fprintf(stderr, "Out of memory!\n");
        throw std::runtime_error("Out of memory!\n");
    }
}

SoftVideoDecoder::~SoftVideoDecoder() {
    ffmpeg_destroy();
    y2rExit();
    linearFree(rgb_img_buffer);
    if (repack_buffer != nullptr) {
        linearFree(repack_buffer);
    }
}

// H.263+ has no NAL units or parameter sets, so the only way to tell that a
// decode unit really is H.263+ (and not damaged, or a different codec) is its
// picture header. Bad units are not handed to FFmpeg.
SoftVideoDecoder::H263Check SoftVideoDecoder::_check_h263_unit(
    PDECODE_UNIT decodeUnit, const u8 *data, int length) {
#if !N3DS_H263_AVAILABLE
    // H.263+ support is not part of this build, so there is nothing to check.
    // This is never reached: VIDEO_FORMAT_MASK_H263 is 0 without H.263+.
    (void)decodeUnit;
    (void)data;
    (void)length;
    return H263Check::PROCEED;
#else
    H263PictureInfo info;
    H263Status status = h263_parse_picture_header(data, length, &info);
    if (status != H263_OK) {
        if (h263_error_count++ < H263_LOG_LIMIT) {
            fprintf(stderr, "Invalid H.263+ picture (%s), requesting IDR\n",
                    h263_status_str(status));
        }
        return H263Check::NEED_IDR;
    }

    if (decodeUnit->frameType == FRAME_TYPE_IDR && !info.is_intra &&
        !h263_idr_mismatch_logged) {
        h263_idr_mismatch_logged = true;
        fprintf(stderr, "Host flagged an IDR frame that is not INTRA coded\n");
    }

    // The renderer and Y2R are set up for the configured size. The size only
    // comes with some pictures, but when it does it must match.
    if (info.has_dimensions && (info.width != (unsigned)image_width ||
                                info.height != (unsigned)image_height)) {
        if (!h263_size_mismatch_logged) {
            h263_size_mismatch_logged = true;
            fprintf(stderr, "Host is sending %ux%u H.263+, expected %dx%d\n",
                    info.width, info.height, image_width, image_height);
        }
        return H263Check::SKIP;
    }
    return H263Check::PROCEED;
#endif
}

// The Y2R transfers in _write_yuv_to_framebuffer assume the planes have no
// padding at the end of each line. FFmpeg pads lines to the alignment of the
// build (and the H.263 decoder is less predictable about it than H.264), so
// copy the planes into a tightly packed buffer when they are padded.
bool SoftVideoDecoder::_repack_planes(const u8 *const src[3],
                                      const int stride[3], const u8 *out[3]) {
    const int chroma_w = image_width / 2;
    const int chroma_h = image_height / 2;
    const size_t y_size = (size_t)image_width * image_height;
    const size_t c_size = (size_t)chroma_w * chroma_h;

    if (repack_buffer == nullptr) {
        repack_buffer = (u8 *)linearAlloc(y_size + 2 * c_size);
        if (repack_buffer == nullptr) {
            fprintf(stderr, "Out of memory!\n");
            return false;
        }
    }

    u8 *dst[3] = {repack_buffer, repack_buffer + y_size,
                  repack_buffer + y_size + c_size};
    const int widths[3] = {image_width, chroma_w, chroma_w};
    const int heights[3] = {image_height, chroma_h, chroma_h};
    for (int p = 0; p < 3; p++) {
        for (int line = 0; line < heights[p]; line++) {
            memcpy(dst[p] + (size_t)line * widths[p],
                   src[p] + (size_t)line * stride[p], widths[p]);
        }
        out[p] = dst[p];
    }

    // Y2R reads this over DMA, so the CPU's writes must reach memory first
    GSPGPU_FlushDataCache(repack_buffer, y_size + 2 * c_size);
    return true;
}

inline int SoftVideoDecoder::_write_yuv_to_framebuffer(const u8 **source,
                                                       int width, int height,
                                                       int px_size) {
    Handle conversion_finish_event_handle;
    int status = 0;
    u64 start_ticks = svcGetSystemTick();

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

    status = Y2RU_SetReceiving(
        rgb_img_buffer, MOON_CTR_VIDEO_TEX_W * MOON_CTR_VIDEO_TEX_H * px_size,
        width * px_size, (MOON_CTR_VIDEO_TEX_W - width) * px_size);
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

    renderer_lock.lock();
    renderer->set_perf_decode_ticks(svcGetSystemTick() - start_ticks);
    renderer->write_px_to_framebuffer(rgb_img_buffer);
    renderer_lock.unlock();

    return DR_OK;

y2ru_failed:
    return -1;
}

int SoftVideoDecoder::submit_decode_unit(PDECODE_UNIT decodeUnit) {
    PLENTRY entry = decodeUnit->bufferList;
    int length = 0;

    ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size,
                    decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE);

    while (entry != NULL) {
        memcpy(ffmpeg_buffer + length, entry->data, entry->length);
        length += entry->length;
        entry = entry->next;
    }
    // FFmpeg requires the padding after the packet to be zeroed
    memset((u8 *)ffmpeg_buffer + length, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    if (video_format & VIDEO_FORMAT_MASK_H263) {
        switch (_check_h263_unit(decodeUnit, (const u8 *)ffmpeg_buffer,
                                 length)) {
        case H263Check::NEED_IDR:
            return DR_NEED_IDR;
        case H263Check::SKIP:
            return DR_OK;
        case H263Check::PROCEED:
            break;
        }
    }

    if (ffmpeg_decode((unsigned char *)ffmpeg_buffer, length) < 0) {
        // The decoder state is unknown, so ask the host for a key frame
        return DR_NEED_IDR;
    }

    AVFrame *frame = ffmpeg_get_frame(false);
    if (frame == NULL) {
        // The decoder did not produce a picture for this unit
        return DR_OK;
    }

    if (frame->width != image_width || frame->height != image_height) {
        if (!frame_size_mismatch_logged) {
            frame_size_mismatch_logged = true;
            fprintf(stderr, "Decoded %dx%d frame, expected %dx%d\n",
                    frame->width, frame->height, image_width, image_height);
        }
        return DR_OK;
    }

    const u8 *planes[3] = {frame->data[0], frame->data[1], frame->data[2]};
    const int chroma_w = image_width / 2;
    if (frame->linesize[0] != image_width || frame->linesize[1] != chroma_w ||
        frame->linesize[2] != chroma_w) {
        const u8 *packed[3];
        if (!_repack_planes(planes, frame->linesize, packed)) {
            return DR_OK;
        }
        memcpy(planes, packed, sizeof(planes));
    }

    int status = _write_yuv_to_framebuffer(planes, image_width, image_height,
                                           pixel_size);

    return status;
}

static int soft_video_setup(int videoFormat, int width, int height,
                            int redrawRate, void *context, int drFlags) {
    try {
        instance = std::make_unique<SoftVideoDecoder>(
            videoFormat, width, height, redrawRate, context, drFlags);
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "Failed to initialize N3DS soft decoder: %s\n",
                e.what());
        return -1;
    }
}

static void soft_video_cleanup() { instance = nullptr; }

static int soft_video_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    if (instance == nullptr) {
        return DR_OK;
    }
    return instance->submit_decode_unit(decodeUnit);
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds = {
    .setup = soft_video_setup,
    .cleanup = soft_video_cleanup,
    .submitDecodeUnit = soft_video_submit_decode_unit,
    .capabilities =
        CAPABILITY_DIRECT_SUBMIT | CAPABILITY_REFERENCE_FRAME_INVALIDATION_AVC,
};
