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

// General decoder and renderer state
static void* n3ds_buffer;
static size_t n3ds_buffer_size;
static MVDSTD_Config mvdstd_config;

static int image_width, image_height, surface_width, surface_height, pixel_size;
static u8* img_buffer;

static int n3ds_init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  int status = mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264, MVD_OUTPUT_BGR565, width * height * N3DS_DEC_BUFF_SIZE, NULL);
  if (status) {
    fprintf(stderr, "mvdstdInit failed: %d\n", status);
    mvdstdExit();
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
  image_width = height;
  image_height = width;
  pixel_size = gspGetBytesPerPixel(px_fmt);
  img_buffer = linearMemAlign(width * height * pixel_size, 0x80);
  if (!img_buffer) {
    fprintf(stderr, "Out of memory!\n");
    return -1;
  }

  ensure_linear_buf_size(&n3ds_buffer, &n3ds_buffer_size, INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
  mvdstdGenerateDefaultConfig(&mvdstd_config, width, height, width, height, NULL, img_buffer, NULL);
  MVDSTD_SetConfig(&mvdstd_config);

  return init_px_to_framebuffer(surface_width, surface_height, image_height, image_width, pixel_size);
}

// This function must be called after
// decoding is finished
static void n3ds_destroy(void) {
  mvdstdExit();
  linearFree(n3ds_buffer);
  linearFree(img_buffer);
  deinit_px_to_framebuffer();
}

static inline void mvd_frame_set_busy(unsigned char* framebuf) {
  *framebuf = 0x11;
  *(framebuf + (image_width * pixel_size - 1)) != 0x11;
  *(framebuf + ((image_width * image_height * pixel_size) - (image_width * pixel_size))) != 0x11;
  *(framebuf + (image_width * image_height * pixel_size - 1)) != 0x11;
}

static inline bool mvd_frame_ready(unsigned char* framebuf) {
  if (mvdstdRenderVideoFrame(&mvdstd_config, false) != MVD_STATUS_BUSY) {
    return true;
  }

  if(*framebuf != 0x11
  || *(framebuf + (image_width * pixel_size - 1)) != 0x11
  || *(framebuf + ((image_width * image_height * pixel_size) - (image_width * pixel_size))) != 0x11
  || *(framebuf + (image_width * image_height * pixel_size - 1)) != 0x11)
  {
    return true;
  }

  return false;
}

// packets must be decoded in order
// indata must be inlen + AV_INPUT_BUFFER_PADDING_SIZE in length
static inline int n3ds_decode(unsigned char* indata, int inlen) {
  int ret = mvdstdProcessVideoFrame(indata, inlen, 1, NULL);
  if(ret!=MVD_STATUS_PARAMSET && ret!=MVD_STATUS_INCOMPLETEPROCESSING)
  {
    mvdstdRenderVideoFrame(&mvdstd_config, false);
  }
  return 0;
}

static int n3ds_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  PLENTRY entry = decodeUnit->bufferList;
  int length = 0;

  ensure_linear_buf_size(&n3ds_buffer, &n3ds_buffer_size, decodeUnit->fullLength + AV_INPUT_BUFFER_PADDING_SIZE);

  while (entry != NULL) {
    memcpy(n3ds_buffer+length, entry->data, entry->length);
    length += entry->length;
    entry = entry->next;
  }
  GSPGPU_FlushDataCache(n3ds_buffer, length);

  u8 *gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
  while (!mvd_frame_ready(img_buffer)) {
    svcSleepThread(1000);
  }
  write_px_to_framebuffer(gfxtopadr, img_buffer, pixel_size);
  gfxScreenSwapBuffers(GFX_TOP, false);

  mvd_frame_set_busy(img_buffer);
  n3ds_decode(n3ds_buffer, length);

  return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds_mvd = {
  .setup = n3ds_init,
  .cleanup = n3ds_destroy,
  .submitDecodeUnit = n3ds_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT,
};
