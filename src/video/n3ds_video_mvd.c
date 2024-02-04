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

#include "video.h"
#include "../util.h"

#include <3ds.h>

#include <Limelight.h>
#include <libavcodec/avcodec.h>

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

#define N3DS_DEC_BUFF_SIZE 23
// Best performing transfer size (optimized through experimentation)
#define N3DS_YUYV_XFER_UNIT 800
// Wait up to 20ms for YUYV conversion to complete (optimized through experimentation)
#define N3DS_YUYV_CONV_WAIT_NS 20000000

// General decoder and renderer state
static void* nal_unit_buffer;
static size_t nal_unit_buffer_size;
static MVDSTD_Config mvdstd_config;
Handle conversion_finish_event_handle = NULL;

static int image_width, image_height, surface_width, surface_height, pixel_size;
static u8* yuv_img_buffer;
static u8* rgb_img_buffer;
static bool first_frame = true;

static int n3ds_init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  bool is_new_3ds;
  APT_CheckNew3DS(&is_new_3ds);
  if (!is_new_3ds) {
    fprintf(stderr, "Hardware decoding is only available on the New 3DS\n");
    return -1;
  }

  int status = mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264, MVD_OUTPUT_YUYV422, width * height * N3DS_DEC_BUFF_SIZE, NULL);
  if (status) {
    fprintf(stderr, "mvdstdInit failed: %d\n", status);
    mvdstdExit();
    return -1;
  }

  if(y2rInit())
  {
    fprintf(stderr, "Failed to initialize Y2R\n");
    return -1;
  }
  Y2RU_ConversionParams y2r_parameters;
  y2r_parameters.input_format = INPUT_YUV422_BATCH;
  y2r_parameters.output_format = OUTPUT_RGB_16_565;
  y2r_parameters.rotation = ROTATION_NONE;
  y2r_parameters.block_alignment = BLOCK_LINE;
  y2r_parameters.input_line_width = width;
  y2r_parameters.input_lines = height;
  y2r_parameters.standard_coefficient = COEFFICIENT_ITU_R_BT_709_SCALING;
  y2r_parameters.alpha = 0xFF;
  status = Y2RU_SetConversionParams(&y2r_parameters);
  if (status) {
    fprintf(stderr, "Failed to set Y2RU params\n");
    return -1;
  }
  status = Y2RU_SetTransferEndInterrupt(true);
  if (status) {
    fprintf(stderr, "Failed to enable Y2RU interrupt\n");
    return -1;
  }

  surface_height = GSP_SCREEN_WIDTH;
  if (width > GSP_SCREEN_HEIGHT_TOP) {
    gfxSetWide(true);
    surface_width = GSP_SCREEN_HEIGHT_TOP_2X;
  }
  else {
    gfxSetWide(false);
    surface_width = GSP_SCREEN_HEIGHT_TOP;
  }

  GSPGPU_FramebufferFormat px_fmt = gfxGetScreenFormat(GFX_TOP);
  image_width = width;
  image_height = height;
  pixel_size = gspGetBytesPerPixel(px_fmt);
  yuv_img_buffer = linearAlloc(width * height * pixel_size);
  if (!yuv_img_buffer) {
    fprintf(stderr, "Out of memory!\n");
    return -1;
  }
  rgb_img_buffer = linearAlloc(width * height * pixel_size);
  if (!rgb_img_buffer) {
    fprintf(stderr, "Out of memory!\n");
    return -1;
  }

  ensure_linear_buf_size(&nal_unit_buffer, &nal_unit_buffer_size, INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
  mvdstdGenerateDefaultConfig(&mvdstd_config, image_width, image_height, image_width, image_height, NULL, yuv_img_buffer, NULL);
  MVDSTD_SetConfig(&mvdstd_config);

  return init_px_to_framebuffer(surface_width, surface_height, image_width, image_height, pixel_size);
}

// This function must be called after
// decoding is finished
static void n3ds_destroy(void) {
  y2rExit();
  mvdstdExit();
  linearFree(nal_unit_buffer);
  linearFree(yuv_img_buffer);
  linearFree(rgb_img_buffer);
  deinit_px_to_framebuffer();
}

static inline int yuv_to_rgb(u8 *dest, const u8 *source, int width, int height, int px_size) {
  int status = Y2RU_SetSendingYUYV(source, width * height * 2, N3DS_YUYV_XFER_UNIT, 0);
  if (status) {
    fprintf(stderr, "Y2RU_SetSendingYUYV failed\n");
    goto y2ru_failed;
  }

  status = Y2RU_SetReceiving(dest, width * height * px_size, 8, 0);
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
  return DR_OK;

  y2ru_failed:
  return -1;
}

// packets must be decoded in order
// indata must be inlen + AV_INPUT_BUFFER_PADDING_SIZE in length
static inline int n3ds_decode(unsigned char* indata, int inlen) {
  int ret = mvdstdProcessVideoFrame(indata, inlen, 1, NULL);
  if(ret!=MVD_STATUS_PARAMSET && ret!=MVD_STATUS_INCOMPLETEPROCESSING)
  {
    mvdstdRenderVideoFrame(&mvdstd_config, true);
  }
  return 0;
}

static int n3ds_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  PLENTRY entry = decodeUnit->bufferList;
  int length = 0;

  ensure_linear_buf_size(&nal_unit_buffer, &nal_unit_buffer_size, decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE);

  while (entry != NULL) {
    memcpy(nal_unit_buffer+length, entry->data, entry->length);
    length += entry->length;
    entry = entry->next;
  }
  GSPGPU_FlushDataCache(nal_unit_buffer, length);

  if (conversion_finish_event_handle != NULL) {
    u8 *gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);

    svcWaitSynchronization(conversion_finish_event_handle, N3DS_YUYV_CONV_WAIT_NS);
    svcCloseHandle(conversion_finish_event_handle);

    write_px_to_framebuffer(gfxtopadr, rgb_img_buffer, pixel_size);
    gfxScreenSwapBuffers(GFX_TOP, false);
  }


  n3ds_decode(nal_unit_buffer, length);
  yuv_to_rgb(rgb_img_buffer, yuv_img_buffer, image_width, image_height, pixel_size);

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
