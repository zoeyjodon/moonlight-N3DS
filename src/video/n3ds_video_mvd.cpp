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

#include "video.hpp"

#include <3ds.h>

#include <Limelight.h>
#include <libavcodec/avcodec.h>

#include <memory>
#include <pthread.h>
#include <stdbool.h>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>

static std::unique_ptr<MvdDecoder> instance = nullptr;

MvdDecoder::MvdDecoder(int videoFormat, int width, int height, int redrawRate,
                       void *context, int drFlags)
    : VideoDecoderBase(width, height) {
    bool is_new_3ds;
    APT_CheckNew3DS(&is_new_3ds);
    if (!is_new_3ds) {
        fprintf(stderr, "Hardware decoding is only available on the New 3DS\n");
        throw std::runtime_error("Unsupported hardware");
    }

    // Calculate required buffer size
    MVDSTD_CalculateWorkBufSizeConfig config = {
        0,
    };
    config.level.enable = true;
    config.level.flag = (MVD_CALC_WITH_LEVEL_FLAG_ENABLE_CALC |
                         MVD_CALC_WITH_LEVEL_FLAG_ENABLE_EXTRA_OP |
                         MVD_CALC_WITH_LEVEL_FLAG_UNK);
    config.level.level = MVD_H264_LEVEL_4_2;
    config.width = width;
    config.height = height;
    uint32_t size = 0;
    int status = mvdstdCalculateBufferSize(&config, &size);
    if (status) {
        fprintf(stderr, "mvdstdCalculateBufferSize failed: %d\n", status);
        throw std::runtime_error("mvdstdCalculateBufferSize failed");
    }

    first_frame = true;
    status = mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264,
                        MVD_OUTPUT_BGR565, size, NULL);
    if (status) {
        fprintf(stderr, "mvdstdInit failed: %d\n", status);
        mvdstdExit();
        throw std::runtime_error("mvdstdInit failed");
    }

    rgb_img_buffer = (u8 *)linearAlloc(MOON_CTR_VIDEO_TEX_W *
                                       MOON_CTR_VIDEO_TEX_H * pixel_size);
    if (!rgb_img_buffer) {
        fprintf(stderr, "Out of memory!\n");
        throw std::runtime_error("Out of memory");
    }

    status = ensure_linear_buf_size(&nal_unit_buffer, &nal_unit_buffer_size,
                                    INITIAL_DECODER_BUFFER_SIZE +
                                        AV_INPUT_BUFFER_PADDING_SIZE);
    if (status) {
        fprintf(stderr, "Out of linear memory!\n");
        throw std::runtime_error("Out of linear memory");
    }
    mvdstdGenerateDefaultConfig(&mvdstd_config, width, height, image_width,
                                image_height, NULL, (u32 *)rgb_img_buffer,
                                NULL);

    // Place within the 1024x512 buffer
    mvdstd_config.flag_x104 = 1;
    mvdstd_config.output_width_override = MOON_CTR_VIDEO_TEX_W;
    mvdstd_config.output_height_override = MOON_CTR_VIDEO_TEX_H;
    MVDSTD_SetConfig(&mvdstd_config);
}

// This function must be called after
// decoding is finished
MvdDecoder::~MvdDecoder() {
    y2rExit();
    mvdstdExit();
    linearFree(nal_unit_buffer);
    linearFree(rgb_img_buffer);
    printf("Video decoder shutdown successfully\n");
}

// packets must be decoded in order
// indata must be inlen + AV_INPUT_BUFFER_PADDING_SIZE in length
DecodeReturnStatus MvdDecoder::_decode(unsigned char *indata, int inlen) {
    int ret = mvdstdProcessVideoFrame(indata, inlen, 1, NULL);
    if (!MVD_CHECKNALUPROC_SUCCESS(ret)) {
        return DecodeReturnStatus::ERROR;
    }

    if (ret != MVD_STATUS_PARAMSET && ret != MVD_STATUS_INCOMPLETEPROCESSING) {
        return DecodeReturnStatus::NO_FRAME_PRODUCED;
    }
    ret = mvdstdRenderVideoFrame(&mvdstd_config, true);
    if (ret != MVD_STATUS_OK) {
        return DecodeReturnStatus::ERROR;
    }
    return DecodeReturnStatus::SUCCESS;
}

int MvdDecoder::submit_decode_unit(PDECODE_UNIT decodeUnit) {
    u64 start_ticks = svcGetSystemTick();
    PLENTRY entry = decodeUnit->bufferList;
    int length = 0;

    if (ensure_linear_buf_size(&nal_unit_buffer, &nal_unit_buffer_size,
                               decodeUnit->fullLength +
                                   AV_INPUT_BUFFER_PADDING_SIZE)) {
        printf("Out of linear memory!\n");
        return DR_OK;
    }

    while (entry != NULL) {
        memcpy(nal_unit_buffer + length, entry->data, entry->length);
        length += entry->length;
        entry = entry->next;
    }
    GSPGPU_FlushDataCache(nal_unit_buffer, length);

    _decode((unsigned char *)nal_unit_buffer, length);

    renderer_lock.lock();
    renderer->set_perf_decode_ticks(svcGetSystemTick() - start_ticks);
    renderer->write_px_to_framebuffer(rgb_img_buffer);
    renderer_lock.unlock();

    // If MVD never gets an IDR frame, everything shows up gray
    if (first_frame) {
        first_frame = false;
        return DR_NEED_IDR;
    }
    return DR_OK;
}

static int n3ds_init(int videoFormat, int width, int height, int redrawRate,
                     void *context, int drFlags) {
    try {
        instance = std::make_unique<MvdDecoder>(videoFormat, width, height,
                                                redrawRate, context, drFlags);
        return 0;
    } catch (const std::exception &e) {
        fprintf(stderr, "Failed to initialize N3DS MVD decoder: %s\n",
                e.what());
        return -1;
    }
}

static void n3ds_destroy() { instance = nullptr; }

static int n3ds_submit_decode_unit(PDECODE_UNIT decodeUnit) {
    if (instance == nullptr) {
        return DR_OK;
    }
    return instance->submit_decode_unit(decodeUnit);
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds_mvd = {
    .setup = n3ds_init,
    .cleanup = n3ds_destroy,
    .submitDecodeUnit = n3ds_submit_decode_unit,
    .capabilities = CAPABILITY_DIRECT_SUBMIT,
};
