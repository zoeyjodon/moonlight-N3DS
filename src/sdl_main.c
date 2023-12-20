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
#include <limits.h>
#include <Limelight.h>

static bool done;
static int fullscreen_flags;

SDL_Mutex *mutex;

void sdl_init(int width, int height, bool fullscreen) {
  mutex = SDL_CreateMutex();
  if (!mutex) {
    printf("Couldn't create mutex\n");
    exit(1);
  }
}

void sdl_loop() {
  while(!done) {
#ifdef __3DS__
    done = !aptMainLoop();
#endif
    switch (sdlinput_handle_event(NULL)) {
    case SDL_QUIT_APPLICATION:
      done = true;
      break;
    }
    hidWaitForEvent(HIDEVENT_PAD0, true);
  }

#ifndef __3DS__ // leave SDL running for debug after crash
  SDL_Quit();
#endif
}

#endif /* HAVE_SDL */
