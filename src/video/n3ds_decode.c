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

#include "n3ds_decode.h"

#include <3ds.h>

#include <Limelight.h>
#include <libavcodec/avcodec.h>

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>

// General decoder and renderer state
AVFrame** dec_frames;

int dec_frames_cnt;
int next_frame_out, next_frame_in;
int frame_width, frame_height;

MVDSTD_Config mvdstd_config;

// This function must be called before
// any other decoding functions
int n3ds_init(int width, int height, int buffer_count) {
  dec_frames_cnt = buffer_count;
  frame_width = width;
  frame_height = height;
  dec_frames = malloc(buffer_count * sizeof(AVFrame*));
  if (dec_frames == NULL) {
    fprintf(stderr, "Couldn't allocate frames");
    return -1;
  }

  for (int i = 0; i < dec_frames_cnt; i++) {
    dec_frames[i] = av_frame_alloc();
    if (dec_frames[i] == NULL) {
      fprintf(stderr, "Couldn't allocate frame");
      return -1;
    }

    dec_frames[i]->linesize[0] = width * height * 2;
		dec_frames[i]->data[0] = (u8*)linearAlloc(dec_frames[i]->linesize[0]);
		if(!dec_frames[i]->data[0])
			return -1;
  }

  int status = mvdstdInit(MVDMODE_VIDEOPROCESSING, MVD_INPUT_H264, MVD_OUTPUT_BGR565, width * height * buffer_count, NULL);
  if (status) {
    fprintf(stderr, "mvdstdInit failed: %d\n", status);
    mvdstdExit();
    return -1;
  }
	mvdstdGenerateDefaultConfig(&mvdstd_config, width, height, width, height, NULL, NULL, NULL);

  return 0;
}

// This function must be called after
// decoding is finished
void n3ds_destroy(void) {
  mvdstdExit();
  if (dec_frames) {
    for (int i = 0; i < dec_frames_cnt; i++) {
      if (dec_frames[i])
        linearFree(dec_frames[i]->data[0]);
        av_frame_free(&dec_frames[i]);
    }
  }
}

static inline int increment_frame(int frame_idx)
{
  return (frame_idx+1) % dec_frames_cnt;
}

static inline bool set_frame_not_ready(int frame_idx) {
  *(dec_frames[frame_idx]->data[0]) = 0x11;
  *(dec_frames[frame_idx]->data[0] + (frame_width * 2 - 1)) = 0x11;
  *(dec_frames[frame_idx]->data[0] + ((frame_width * frame_height * 2) - (frame_width * 2))) = 0x11;
  *(dec_frames[frame_idx]->data[0] + (frame_width * frame_height * 2 - 1)) = 0x11;
}

static inline bool frame_ready(int frame_idx) {
  return (*dec_frames[frame_idx]->data[0] != 0x11
  || *(dec_frames[frame_idx]->data[0] + (frame_width * 2 - 1)) != 0x11
  || *(dec_frames[frame_idx]->data[0] + ((frame_width * frame_height * 2) - (frame_width * 2))) != 0x11
  || *(dec_frames[frame_idx]->data[0] + (frame_width * frame_height * 2 - 1)) != 0x11);
}

AVFrame* n3ds_get_frame(bool native_frame) {
  if (next_frame_in == next_frame_out) {
    // No frame available.
    return NULL;
  }

  if (!frame_ready(next_frame_out)) {
    // Wait for frame to be ready
    mvdstdRenderVideoFrame(&mvdstd_config, false);
    if (!frame_ready(next_frame_out)) {
      printf("Frame is unavailable.");
      return NULL;
    }
  }

  int current_frame_out = next_frame_out;
  next_frame_out = increment_frame(next_frame_out);
  return dec_frames[current_frame_out];
}

// packets must be decoded in order
// indata must be inlen + AV_INPUT_BUFFER_PADDING_SIZE in length
int n3ds_decode(unsigned char* indata, int inlen) {
  int current_frame_in = next_frame_in;
  next_frame_in = increment_frame(next_frame_in);
  if (next_frame_in == next_frame_out) {
    // Circular buffer is full. Skip oldest frame.
    printf("Frame buffer full, skipping frame...");
    increment_frame(next_frame_out);
    return -1;
  }

  set_frame_not_ready(current_frame_in);
	mvdstd_config.physaddr_outdata0 = osConvertVirtToPhys(dec_frames[current_frame_in]->data[0]);
	MVDSTD_SetConfig(&mvdstd_config);
  int ret = mvdstdProcessVideoFrame(indata, inlen, 0, NULL);
  if(!MVD_CHECKNALUPROC_SUCCESS(ret))
  {
    printf("mvdstdProcessVideoFrame() failed: %d\n", ret);
    return ret;
  }
  if(ret==MVD_STATUS_PARAMSET || ret==MVD_STATUS_INCOMPLETEPROCESSING)
  {
    // No frame will be produced, reuse this buffer
    next_frame_in = current_frame_in;
  }
  return 0;
}
