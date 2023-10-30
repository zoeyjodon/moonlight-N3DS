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

#include "n3ds.h"

#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>

#include <stdio.h>
#include <opus/opus_multistream.h>

static OpusMSDecoder* decoder;
static short* pcmBuffer;
static int samplesPerFrame;
static int channelCount;

struct sample {
    Uint8 *data;
    Uint32 dpos;
    Uint32 dlen;
} audio_stream;

void sdl_callback(void *unused, Uint8 *stream, int len)
{
  int i;
  Uint32 amount;

  amount = (audio_stream.dlen-audio_stream.dpos);
  if ( amount > len ) {
      amount = len;
  }
  SDL_MixAudio(stream, &audio_stream.data[audio_stream.dpos], amount, SDL_MIX_MAXVOLUME);
  audio_stream.dpos += amount;
}

static int sdl_renderer_init(int audioConfiguration, POPUS_MULTISTREAM_CONFIGURATION opusConfig, void* context, int arFlags) {
  int rc;
  decoder = opus_multistream_decoder_create(opusConfig->sampleRate, opusConfig->channelCount, opusConfig->streams, opusConfig->coupledStreams, opusConfig->mapping, &rc);

  channelCount = opusConfig->channelCount;
  samplesPerFrame = opusConfig->samplesPerFrame;
  pcmBuffer = malloc(sizeof(short) * channelCount * samplesPerFrame);
  if (pcmBuffer == NULL)
    return -1;

  SDL_InitSubSystem(SDL_INIT_AUDIO);

  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = opusConfig->sampleRate;
  want.format = AUDIO_S16LSB;
  want.channels = opusConfig->channelCount;
  want.samples = 4096;
  want.callback = sdl_callback;

  int status = SDL_OpenAudio(&want, &have);
  if (status != 0) {
    printf("Failed to open audio: %d\n", status);
    return -1;
  }

  SDL_PauseAudio(0);  // start audio playing.
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

  SDL_CloseAudio();
}

static void sdl_renderer_decode_and_play_sample(char* data, int length) {
  int decodeLen = opus_multistream_decode(decoder, data, length, pcmBuffer, samplesPerFrame, 0);
  if (decodeLen <= 0) {
    printf("Opus error from decode: %d\n", decodeLen);
    return;
  }

  SDL_LockAudio();
  if ( audio_stream.data ) {
      free(audio_stream.data);
  }
  audio_stream.data = pcmBuffer;
  audio_stream.dlen = decodeLen * channelCount * sizeof(short);
  audio_stream.dpos = 0;
  SDL_UnlockAudio();
}

AUDIO_RENDERER_CALLBACKS audio_callbacks_n3ds = {
  .init = sdl_renderer_init,
  .cleanup = sdl_renderer_cleanup,
  .decodeAndPlaySample = sdl_renderer_decode_and_play_sample,
  .capabilities = CAPABILITY_DIRECT_SUBMIT | CAPABILITY_SUPPORTS_ARBITRARY_AUDIO_DURATION,
};
