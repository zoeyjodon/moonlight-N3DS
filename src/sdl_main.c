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
SDL_Mutex *mutex;

void sdl_init(int width, int height, bool fullscreen) {

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
    }
  }

  SDL_DestroyWindow(window);
#ifndef __3DS__ // leave SDL running for debug after crash
  SDL_Quit();
#endif
}

#endif /* HAVE_SDL */
