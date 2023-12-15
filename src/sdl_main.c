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

#ifdef HAVE_SDL

#include "sdl_main.h"
#include "input/sdl.h"

#include <3ds.h>
#include <Limelight.h>

static bool done;
static int fullscreen_flags;

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *bmp;

SDL_Mutex *mutex;

int sdlCurrentFrame, sdlNextFrame;
int surface_width, surface_height, pixel_size;
static u8* img_buffer;

void sdl_init(int width, int height, bool fullscreen) {
  sdlCurrentFrame = sdlNextFrame = 0;

  if(SDL_Init(SDL_INIT_EVENTS)) {
    printf("Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

  fullscreen_flags = fullscreen?SDL_WINDOW_FULLSCREEN:0;
  int window_flags = fullscreen_flags;
  window = SDL_CreateWindow("Moonlight", width, height, window_flags);
  if(!window) {
    printf("SDL: could not create window - exiting\n");
    exit(1);
  }

  mutex = SDL_CreateMutex();
  if (!mutex) {
    printf("Couldn't create mutex\n");
    exit(1);
  }

  if(y2rInit())
  {
    printf("Failed to initialize Y2R\n");
    exit(1);
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
    exit(1);
  }

  GSPGPU_FramebufferFormat px_fmt = GSP_RGB565_OES;
  gfxInit(px_fmt, GSP_RGBA8_OES, false);
  surface_width = width;
  surface_height = height;
  pixel_size = gspGetBytesPerPixel(px_fmt);

  img_buffer = linearAlloc(width * height * pixel_size);
  if (!img_buffer) {
    printf("Out of memory!");
    exit(1);
  }
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

static inline void write_yuv_to_framebuffer(u8 *dest, const u8 **source, int width, int height) {
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
  return;

	y2ru_failed:
  done = true;
}

void sdl_loop() {
  SDL_Event event;

  SDL_SetRelativeMouseMode(SDL_TRUE);

  while(!done && SDL_WaitEvent(&event)) {
#ifdef __3DS__
    done = !aptMainLoop();
#endif
    switch (sdlinput_handle_event(window, &event)) {
    case SDL_QUIT_APPLICATION:
      done = true;
      break;
    case SDL_TOGGLE_FULLSCREEN:
      fullscreen_flags ^= SDL_WINDOW_FULLSCREEN;
      SDL_SetWindowFullscreen(window, fullscreen_flags);
      break;
    case SDL_MOUSE_GRAB:
      SDL_ShowCursor();
      SDL_SetRelativeMouseMode(SDL_TRUE);
      break;
    case SDL_MOUSE_UNGRAB:
      SDL_SetRelativeMouseMode(SDL_FALSE);
      SDL_HideCursor();
      break;
    default:
      if (event.type == SDL_EVENT_QUIT)
        done = true;
      else if (event.type == SDL_EVENT_USER) {
        if (event.user.code == SDL_CODE_FRAME) {
          if (++sdlCurrentFrame <= sdlNextFrame - SDL_BUFFER_FRAMES) {
            //Skip frame
          } else {
            SDL_LockMutex(mutex);
            Uint8** data = ((Uint8**) event.user.data1);
            int* linesize = ((int*) event.user.data2);
            u8 *gfxtopadr = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
            write_yuv_to_framebuffer(gfxtopadr, data, surface_width, surface_height);
            SDL_UnlockMutex(mutex);
            gfxScreenSwapBuffers(GFX_TOP, false);
          }
        }
      }
    }
  }

  SDL_DestroyWindow(window);
#ifndef __3DS__ // leave SDL running for debug after crash
  SDL_Quit();
#else
  y2rExit();
  linearFree(img_buffer);
#endif
}

#endif /* HAVE_SDL */
