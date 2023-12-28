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

#include <Limelight.h>
#include <stdio.h>

static bool done;
static int fullscreen_flags;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *bmp = NULL;

SDL_Mutex *mutex;

int sdlCurrentFrame, sdlNextFrame;

void sdl_init(int width, int height, bool fullscreen) {
  sdlCurrentFrame = sdlNextFrame = 0;

#ifndef __3DS__
  Uint32 init_flags = SDL_INIT_VIDEO | SDL_INIT_EVENTS;
#else
  Uint32 init_flags = SDL_INIT_EVENTS;
#endif
  if(SDL_Init(init_flags)) {
    fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
    exit(1);
  }

#ifndef __3DS__
  fullscreen_flags = fullscreen?SDL_WINDOW_FULLSCREEN:0;
  window = SDL_CreateWindow("Moonlight", width, height, SDL_WINDOW_OPENGL | fullscreen_flags);
  if(!window) {
    fprintf(stderr, "SDL: could not create window - exiting\n");
    exit(1);
  }

  renderer = SDL_CreateRenderer(window, NULL, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    printf("Failed to initialize a hardware accelerated renderer: %s\n", SDL_GetError());
    renderer = SDL_CreateRenderer(window, NULL, 0);
    if (!renderer) {
    printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    exit(1);
    }
  }

  bmp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!bmp) {
    fprintf(stderr, "SDL: could not create texture - exiting\n");
    exit(1);
  }
#endif

  mutex = SDL_CreateMutex();
  if (!mutex) {
    fprintf(stderr, "Couldn't create mutex\n");
    exit(1);
  }
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
      if (window) {
        fullscreen_flags ^= SDL_WINDOW_FULLSCREEN;
        SDL_SetWindowFullscreen(window, fullscreen_flags);
      }
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
            SDL_UpdateYUVTexture(bmp, NULL, data[0], linesize[0], data[1], linesize[1], data[2], linesize[2]);
            SDL_UnlockMutex(mutex);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, bmp, NULL, NULL);
            SDL_RenderPresent(renderer);
          }
        }
      }
    }
  }

#ifndef __3DS__ // leave SDL running for debug after crash
  SDL_DestroyWindow(window);
  SDL_Quit();
#endif
}

#endif /* HAVE_SDL */
