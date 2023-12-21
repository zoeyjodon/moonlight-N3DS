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

static int n3ds_init(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  int status = mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264, MVD_OUTPUT_BGR565, width * height * N3DS_DEC_BUFF_SIZE, NULL);
  if (status) {
    fprintf(stderr, "mvdstdInit failed: %d\n", status);
    mvdstdExit();
    return -1;
  }

  ensure_linear_buf_size(&n3ds_buffer, &n3ds_buffer_size, INITIAL_DECODER_BUFFER_SIZE + AV_INPUT_BUFFER_PADDING_SIZE);
	mvdstdGenerateDefaultConfig(&mvdstd_config, width, height, GSP_SCREEN_WIDTH, GSP_SCREEN_HEIGHT_TOP, NULL, NULL, NULL);
	MVDSTD_SetConfig(&mvdstd_config);
  return 0;
}

// This function must be called after
// decoding is finished
static void n3ds_destroy(void) {
  mvdstdExit();
}

// packets must be decoded in order
// indata must be inlen + AV_INPUT_BUFFER_PADDING_SIZE in length
static inline int n3ds_decode(unsigned char* indata, int inlen) {
  int ret = mvdstdProcessVideoFrame(indata, inlen, 0, NULL);
  if(!MVD_CHECKNALUPROC_SUCCESS(ret))
  {
    return ret;
  }
  if(ret!=MVD_STATUS_PARAMSET && ret!=MVD_STATUS_INCOMPLETEPROCESSING)
  {
    u8 *gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    mvdstd_config.physaddr_outdata0 = osConvertVirtToPhys(gfxtopadr);

    mvdstdRenderVideoFrame(&mvdstd_config, false);
    gfxScreenSwapBuffers(GFX_TOP, false);
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
  n3ds_decode(n3ds_buffer, length);

  return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds_mvd = {
  .setup = n3ds_init,
  .cleanup = n3ds_destroy,
  .submitDecodeUnit = n3ds_submit_decode_unit,
  .capabilities = CAPABILITY_REFERENCE_FRAME_INVALIDATION_HEVC | CAPABILITY_DIRECT_SUBMIT,
};
