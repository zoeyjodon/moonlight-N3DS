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

#ifdef __3DS__
#include <3ds.h>
#include <math.h>
#include <opus/opus_multistream.h>
#else
#include <SDL.h>
#include <SDL_audio.h>
#include <opus_multistream.h>
#endif

#include <stdio.h>

#define WAVEBUF_SIZE 3

static OpusMSDecoder* decoder;
static short* pcmBuffer;
static int samplesPerFrame;
static int sampleRate;
static int channelCount;
static ndspWaveBuf audio_wave_buf[WAVEBUF_SIZE];
static int wave_buf_idx = 0;
LightEvent buf_ready_event;


static void AudioFrameFinished(void *_unused)
{
    for (int i = 0; i < WAVEBUF_SIZE; i++) {
        if (audio_wave_buf[i].status == NDSP_WBUF_DONE) {
          wave_buf_idx = i;
          LightEvent_Signal(&buf_ready_event);
          break;
        }
    }
}

static int sdl_renderer_init(int audioConfiguration, POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int arFlags) {
  int rc;
  decoder = opus_multistream_decoder_create(opusConfig->sampleRate, opusConfig->channelCount, opusConfig->streams, opusConfig->coupledStreams, opusConfig->mapping, &rc);

  sampleRate = opusConfig->sampleRate;
  channelCount = opusConfig->channelCount;
  samplesPerFrame = opusConfig->samplesPerFrame;
  int bytes_per_frame = sizeof(short) * channelCount * samplesPerFrame;
  pcmBuffer = malloc(bytes_per_frame);
  if (pcmBuffer == NULL)
    return -1;

	if(ndspInit() != 0)
	{
		printf("ndspInit() failed\n");
    return -1;
	}

	u32 *audioBuffer = (u32*)linearAlloc(bytes_per_frame * WAVEBUF_SIZE);
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

  LightEvent_Init(&buf_ready_event, RESET_ONESHOT);
  ndspSetCallback(AudioFrameFinished, NULL);
	ndspChnSetPaused(0, false);

  return 0;
}

static void sdl_renderer_cleanup() {
  if (decoder != NULL) {
    opus_multistream_decoder_destroy(decoder);
    decoder = NULL;
  }

  if (pcmBuffer != NULL) {
    free(pcmBuffer);
    pcmBuffer = NULL;
  }

	ndspChnWaveBufClear(0);
	ndspExit();
}

static void sdl_renderer_decode_and_play_sample(char* data, int length) {
  int decodeLen = opus_multistream_decode(decoder, data, length, pcmBuffer, samplesPerFrame, 0);
  if (decodeLen < 0) {
    printf("Opus error from decode: %d\n", decodeLen);
    return;
  }

  if (audio_wave_buf[wave_buf_idx].status != NDSP_WBUF_DONE)
  {
    LightEvent_Wait(&buf_ready_event);
  }

  int decodeByteLen = decodeLen * channelCount * sizeof(short);
	memcpy(audio_wave_buf[wave_buf_idx].data_vaddr, pcmBuffer, decodeByteLen);
	DSP_FlushDataCache(audio_wave_buf[wave_buf_idx].data_vaddr, decodeByteLen);

  audio_wave_buf[wave_buf_idx].nsamples = decodeLen;
	ndspChnWaveBufAdd(0, &audio_wave_buf[wave_buf_idx]);

  for (int i = 0; i < WAVEBUF_SIZE; i++) {
    if (audio_wave_buf[i].status == NDSP_WBUF_DONE) {
      wave_buf_idx = i;
      break;
    }
  }
}

AUDIO_RENDERER_CALLBACKS audio_callbacks_sdl = {
  .init = sdl_renderer_init,
  .cleanup = sdl_renderer_cleanup,
  .decodeAndPlaySample = sdl_renderer_decode_and_play_sample,
  .capabilities = 0,
};
