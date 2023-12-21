/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2017 Iwan Timmer
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

#include "audio.h"

#include <3ds.h>
#include <math.h>
#include <opus/opus_multistream.h>
#include <stdio.h>

#define WAVEBUF_SIZE 1024

static OpusMSDecoder* decoder;
static u8* audioBuffer;
static int samplesPerFrame;
static int sampleRate;
static int channelCount;
static ndspWaveBuf audio_wave_buf[WAVEBUF_SIZE];
static int wave_buf_idx = 0;

static int n3ds_renderer_init(int audioConfiguration, POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int arFlags) {
  int rc;
  decoder = opus_multistream_decoder_create(opusConfig->sampleRate, opusConfig->channelCount, opusConfig->streams, opusConfig->coupledStreams, opusConfig->mapping, &rc);

  sampleRate = opusConfig->sampleRate;
  channelCount = opusConfig->channelCount;
  samplesPerFrame = opusConfig->samplesPerFrame;
  int bytes_per_frame = sizeof(short) * channelCount * samplesPerFrame;

	if(ndspInit() != 0)
	{
		printf("ndspInit() failed\n");
    return -1;
	}

	u8 *audioBuffer = (u8*)linearAlloc(bytes_per_frame * WAVEBUF_SIZE);
  if (audioBuffer == NULL)
    return -1;
	memset(audioBuffer, 0, bytes_per_frame * WAVEBUF_SIZE);

	ndspChnWaveBufClear(0);
  ndspChnReset(0);
  ndspSetOutputMode(NDSP_OUTPUT_STEREO);
  ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
  ndspChnSetRate(0, sampleRate);
  ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

	float mix[12];
	memset(mix, 0, sizeof(mix));
  mix[0] = mix[1] = 1.0f;
	ndspChnSetMix(0, mix);

	memset(audio_wave_buf,0,sizeof(audio_wave_buf));
  for (int i = 0; i < WAVEBUF_SIZE; i++) {
    audio_wave_buf[i].data_vaddr = &audioBuffer[i * bytes_per_frame];
    audio_wave_buf[i].status = NDSP_WBUF_DONE;
  }

	ndspChnSetPaused(0, false);

  return 0;
}

static void n3ds_renderer_cleanup() {
  if (decoder != NULL) {
    opus_multistream_decoder_destroy(decoder);
    decoder = NULL;
  }

	ndspChnWaveBufClear(0);
	ndspExit();
  if (audioBuffer != NULL) {
    free(audioBuffer);
    audioBuffer = NULL;
  }
}

static void n3ds_renderer_decode_and_play_sample(char* data, int length) {
  if (audio_wave_buf[wave_buf_idx].status != NDSP_WBUF_DONE)
  {
    // Buffer is full, drop the frame
    opus_multistream_decode(decoder, NULL, 0, NULL, samplesPerFrame, 0);
    return;
  }

  int decodeLen = opus_multistream_decode(decoder, data, length, audio_wave_buf[wave_buf_idx].data_vaddr, samplesPerFrame, 0);
  if (decodeLen < 0) {
    printf("Opus error from decode: %d\n", decodeLen);
    return;
  }
	DSP_FlushDataCache(audio_wave_buf[wave_buf_idx].data_vaddr, decodeLen * channelCount * sizeof(short));
  audio_wave_buf[wave_buf_idx].nsamples = decodeLen;
	ndspChnWaveBufAdd(0, &audio_wave_buf[wave_buf_idx]);

  wave_buf_idx = (wave_buf_idx +  1) % WAVEBUF_SIZE;
}

AUDIO_RENDERER_CALLBACKS audio_callbacks_n3ds = {
  .init = n3ds_renderer_init,
  .cleanup = n3ds_renderer_cleanup,
  .decodeAndPlaySample = n3ds_renderer_decode_and_play_sample,
  .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SLOW_OPUS_DECODER,
};
