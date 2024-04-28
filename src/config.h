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

#pragma once

#include <Limelight.h>

#include <stdbool.h>
#include "platform_main.h"

#define MAX_INPUTS 6

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _CONFIGURATION {
  STREAM_CONFIGURATION stream;
  int debug_level;
  char* app;
  char* action;
  char* address;
  char* mapping;
  char* platform;
  char* audio_device;
  char* config_file;
  char key_dir[4096];
  bool sops;
  bool localaudio;
  bool fullscreen;
  int rotate;
  bool unsupported;
  bool quitappafter;
  bool viewonly;
  bool mouse_emulation;
  char* inputs[MAX_INPUTS];
  int inputsCount;
  enum codecs codec;
  bool hdr;
  int pin;
  unsigned short port;
  bool hwdecode;
  bool dual_screen;
  bool motion_controls;
  bool swap_face_buttons;
  bool swap_triggers_and_shoulders;
} CONFIGURATION, *PCONFIGURATION;

extern bool inputAdded;

bool config_file_parse(char* filename, PCONFIGURATION config);
void config_parse(int argc, char* argv[], PCONFIGURATION config);
void parse_argument(int c, char* value, PCONFIGURATION config);
void config_save(char* filename, PCONFIGURATION config);

#ifdef __cplusplus
}
#endif
