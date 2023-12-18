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

#include "video.h"
#include "ffmpeg.h"

#include "../sdl_main.h"
#include "../util.h"

#ifdef __3DS__
#include <3ds.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_thread.h>
#else
#include <SDL.h>
#include <SDL_thread.h>
#endif

#include <unistd.h>
#include <stdbool.h>

#define SLICES_PER_FRAME 4

static void* ffmpeg_buffer;
static size_t ffmpeg_buffer_size;
static int surface_width, surface_height, pixel_size;
static u8* img_buffer;
static LightLock render_mutex;

static int sdl_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  if (ffmpeg_init(videoFormat, width, height, SLICE_THREADING, SDL_BUFFER_FRAMES, SLICES_PER_FRAME) < 0) {
    fprintf(stderr, "Couldn't initialize video decoding\n");
    return -1;
  }

  ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size, INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);

  LightLock_Init(&render_mutex);

  if(y2rInit())
  {
    printf("Failed to initialize Y2R\n");
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
    printf("Failed to set Y2RU params\n");
    return -1;
  }

  GSPGPU_FramebufferFormat px_fmt = GSP_RGB565_OES;
  gfxInit(px_fmt, GSP_RGB565_OES, false);
  surface_width = width;
  surface_height = height;
  pixel_size = gspGetBytesPerPixel(px_fmt);

  img_buffer = linearAlloc(width * height * pixel_size);
  if (!img_buffer) {
    printf("Out of memory!");
    return -1;
  }

  return 0;
}

static void sdl_cleanup() {
  ffmpeg_destroy();
  y2rExit();
  linearFree(img_buffer);
}

static inline int get_dest_offset(int x, int y, int height)
{
  return height - y - 1 + height * x;
}

static inline int get_source_offset(int x, int y, int width)
{
  return x + y * width;
}

static inline void write_rgb565_to_framebuffer(u16* dest, u16* source, int width, int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int src_offset = get_source_offset(x, y, width);
      int dst_offset = get_dest_offset(x, y, height);
      dest[dst_offset] = source[src_offset];
    }
  }
}

static inline int write_yuv_to_framebuffer(u8 *dest, const u8 **source, int width, int height) {
	Handle conversion_finish_event_handle;
  int status = 0;

  status = Y2RU_SetSendingY(source[0], width * height, width, 0);
  if (status) {
    printf("Y2RU_SetSendingY failed\n");
    goto y2ru_failed;
  }

  status = Y2RU_SetSendingU(source[1], width * height / 4, width / 2, 0);
  if (status) {
    printf("Y2RU_SetSendingU failed\n");
    goto y2ru_failed;
  }

  status = Y2RU_SetSendingV(source[2], width * height / 4, width / 2, 0);
  if (status) {
    printf("Y2RU_SetSendingV failed\n");
    goto y2ru_failed;
  }

  status = Y2RU_SetReceiving(img_buffer, width * height * pixel_size, width * pixel_size * 4, 0);
  if (status) {
    printf("Y2RU_SetReceiving failed\n");
    goto y2ru_failed;
  }

  status = Y2RU_StartConversion();
  if (status) {
    printf("Y2RU_StartConversion failed\n");
    goto y2ru_failed;
  }

  status = Y2RU_GetTransferEndEvent(&conversion_finish_event_handle);
  if (status) {
    printf("Y2RU_GetTransferEndEvent failed\n");
    goto y2ru_failed;
  }

  svcWaitSynchronization(conversion_finish_event_handle, 200000000);//Wait up to 200ms.
  svcCloseHandle(conversion_finish_event_handle);
  write_rgb565_to_framebuffer(dest, img_buffer, width, height);
  return DR_OK;

	y2ru_failed:
  return -1;
}

static int sdl_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  PLENTRY entry = decodeUnit->bufferList;
  int length = 0;

  ensure_buf_size(&ffmpeg_buffer, &ffmpeg_buffer_size, decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE);

  while (entry != NULL) {
    memcpy(ffmpeg_buffer+length, entry->data, entry->length);
    length += entry->length;
    entry = entry->next;
  }
  ffmpeg_decode(ffmpeg_buffer, length);

  AVFrame* frame = ffmpeg_get_frame(false);
  u8 *gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
  int status = write_yuv_to_framebuffer(gfxtopadr, frame->data, surface_width, surface_height);
  gfxScreenSwapBuffers(GFX_TOP, false);

  return status;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_sdl = {
  .setup = sdl_setup,
  .cleanup = sdl_cleanup,
  .submitDecodeUnit = sdl_submit_decode_unit,
  .capabilities = CAPABILITY_SLICES_PER_FRAME(SLICES_PER_FRAME) | CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC,
};
