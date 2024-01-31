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
#define N3DS_BUFFER_COUNT 2

// General decoder and renderer state
static void* nal_unit_buffer;
static size_t nal_unit_buffer_size;
static MVDSTD_Config mvdstd_config;

static int image_width, image_height, surface_width, surface_height, pixel_size;
static u8* yuv_img_buffers[N3DS_BUFFER_COUNT];
static int yuv_in_idx, yuv_out_idx;
static u8* rgb_img_buffers[N3DS_BUFFER_COUNT];
static int rgb_in_idx, rgb_out_idx;
static bool first_frame = true;

static Thread yuv_thread, rgb_thread;
static bool yuv_thread_active = true;
static bool rgb_thread_active = true;
static bool yuv_buffer_empty[N3DS_BUFFER_COUNT];
static bool rgb_buffer_empty[N3DS_BUFFER_COUNT];

static void yuv_processing_loop(void* context);
static void rgb_processing_loop(void* context);

static int n3ds_init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
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

  surface_height = 240;
  if (width > 400) {
    gfxSetWide(true);
    surface_width = 800;
  }
  else {
    gfxSetWide(false);
    surface_width = 400;
  }

  GSPGPU_FramebufferFormat px_fmt = gfxGetScreenFormat(GFX_TOP);
  image_width = width;
  image_height = height;
  pixel_size = gspGetBytesPerPixel(px_fmt);

  yuv_in_idx = 0;
  yuv_out_idx = 0;
  rgb_in_idx = 0;
  rgb_out_idx = 0;
  for (int i = 0; i < N3DS_BUFFER_COUNT; i++) {
    yuv_img_buffers[i] = linearMemAlign(width * height * 2, 0x80);
    if (!yuv_img_buffers[i]) {
      fprintf(stderr, "Out of memory!\n");
      return -1;
    }
    yuv_buffer_empty[i] = true;

    rgb_img_buffers[i] = linearMemAlign(width * height * pixel_size, 0x80);
    if (!rgb_img_buffers[i]) {
      fprintf(stderr, "Out of memory!\n");
      return -1;
    }
    rgb_buffer_empty[i] = true;
  }
  ensure_linear_buf_size(&nal_unit_buffer, &nal_unit_buffer_size, INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
  mvdstdGenerateDefaultConfig(&mvdstd_config, image_width, image_height, surface_width, surface_height, NULL, NULL, NULL);
  MVDSTD_SetConfig(&mvdstd_config);

  status = init_px_to_framebuffer(surface_width, surface_height, surface_width, surface_height, pixel_size);
  if (status) {
    return -1;
  }

  // Create the YUV processing thread
  int priority = 0x30;
  svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
  yuv_thread = threadCreate(yuv_processing_loop,
                            NULL,
                            0x10000,
                            priority,
                            1,
                            false);
  if (yuv_thread == NULL) {
      return -1;
  }

  // Create the pixel display thread
  rgb_thread = threadCreate(rgb_processing_loop,
                            NULL,
                            0x10000,
                            priority,
                            2,
                            false);
  if (rgb_thread == NULL) {
      return -1;
  }
  return 0;
}

// This function must be called after
// decoding is finished
static void n3ds_destroy(void) {
  if (yuv_thread) {
    yuv_thread_active = false;
    threadJoin(yuv_thread, U64_MAX);
    threadFree(yuv_thread);
  }
  if (rgb_thread) {
    rgb_thread_active = false;
    threadJoin(rgb_thread, U64_MAX);
    threadFree(rgb_thread);
  }
  y2rExit();
  mvdstdExit();
  linearFree(nal_unit_buffer);
  for (int i = 0; i < N3DS_BUFFER_COUNT; i++) {
    linearFree(yuv_img_buffers[i]);
    linearFree(rgb_img_buffers[i]);
  }
  deinit_px_to_framebuffer();
}

static inline int yuv_to_rgb(u8 *dest, const u8 *source, int width, int height, int px_size) {
  Handle conversion_finish_event_handle;
  int status = 0;

  status = Y2RU_SetSendingYUYV(source, width * height * 2, 800, 0);
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

  svcWaitSynchronization(conversion_finish_event_handle, 20000000);//Wait up to 20ms.
  svcCloseHandle(conversion_finish_event_handle);
  return DR_OK;

  y2ru_failed:
  return -1;
}

static inline void mvd_frame_set_busy(unsigned char* framebuf) {
  *framebuf = 0x11;
  *(framebuf + (image_width * pixel_size - 1)) = 0x11;
  *(framebuf + ((image_width * image_height * pixel_size) - (image_width * pixel_size))) = 0x11;
  *(framebuf + (image_width * image_height * pixel_size - 1)) = 0x11;
}

static inline bool mvd_frame_ready(unsigned char* framebuf) {
  if(*framebuf != 0x11
  || *(framebuf + (image_width * pixel_size - 1)) != 0x11
  || *(framebuf + ((image_width * image_height * pixel_size) - (image_width * pixel_size))) != 0x11
  || *(framebuf + (image_width * image_height * pixel_size - 1)) != 0x11)
  {
    return true;
  }

  return false;
}

static void rgb_processing_loop(void* context) {
    while (rgb_thread_active) {
      if (rgb_buffer_empty[rgb_in_idx]) {
        svcSleepThread(20000);
        continue;
      }
      u8 *gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
      write_px_to_framebuffer(gfxtopadr, rgb_img_buffers[rgb_in_idx], pixel_size);
      rgb_buffer_empty[rgb_in_idx] = true;
      rgb_in_idx = (rgb_in_idx + 1) % N3DS_BUFFER_COUNT;

      gfxScreenSwapBuffers(GFX_TOP, false);
    }
}

static void yuv_processing_loop(void* context) {
    while (yuv_thread_active) {
      if (yuv_buffer_empty[yuv_in_idx] || !rgb_buffer_empty[rgb_out_idx] || !mvd_frame_ready(yuv_img_buffers[yuv_in_idx])) {
        svcSleepThread(20000);
        continue;
      }
      yuv_to_rgb(rgb_img_buffers[rgb_out_idx], yuv_img_buffers[yuv_in_idx], surface_width, surface_height, pixel_size);
      yuv_buffer_empty[yuv_in_idx] = true;
      rgb_buffer_empty[rgb_out_idx] = false;
      yuv_in_idx = (yuv_in_idx + 1) % N3DS_BUFFER_COUNT;
      rgb_out_idx = (rgb_out_idx + 1) % N3DS_BUFFER_COUNT;
    }
}

// packets must be decoded in order
// src must be src_size + AV_INPUT_BUFFER_PADDING_SIZE in length
static inline int nal_to_yuv_decode(unsigned char* dest, unsigned char* src, int src_size) {
  int ret = mvdstdProcessVideoFrame(src, src_size, 1, NULL);
  if(ret!=MVD_STATUS_PARAMSET && ret!=MVD_STATUS_INCOMPLETEPROCESSING)
  {
    mvdstd_config.physaddr_outdata0 = osConvertVirtToPhys(dest);
    mvdstdRenderVideoFrame(&mvdstd_config, false);
    yuv_buffer_empty[yuv_out_idx] = false;
    yuv_out_idx = (yuv_out_idx + 1) % N3DS_BUFFER_COUNT;
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

  while (!yuv_buffer_empty[yuv_out_idx]) {
    svcSleepThread(20000);
  }
  mvd_frame_set_busy(yuv_img_buffers[yuv_out_idx]);
  nal_to_yuv_decode(yuv_img_buffers[yuv_out_idx], nal_unit_buffer, length);

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
