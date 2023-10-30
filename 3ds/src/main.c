/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2019 Iwan Timmer
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

#include "loop.h"
#include "connection.h"
#include "config.h"
#include "platform.h"

#include <Limelight.h>

#include <client.h>
#include <discover.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "3ds.h"
#include <whb/gfx.h>
#include <vpad/input.h>

#ifdef DEBUG
void Debug_Init();
#endif

#define SCREEN_BAR "\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501" \
  "\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501" \
  "\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\u2501\n"

int state = STATE_INVALID;

int is_error = 0;
char message_buffer[1024] = "\0";

int autostream = 0;

static void applist(PSERVER_DATA server) {
  PAPP_LIST list = NULL;
  if (gs_applist(server, &list) != GS_OK) {
    fprintf(stderr, "Can't get app list\n");
    return;
  }

  for (int i = 1;list != NULL;i++) {
    printf("%d. %s\n", i, list->name);
    list = list->next;
  }
}

static int get_app_id(PSERVER_DATA server, const char *name) {
  PAPP_LIST list = NULL;
  if (gs_applist(server, &list) != GS_OK) {
    fprintf(stderr, "Can't get app list\n");
    return -1;
  }

  while (list != NULL) {
    if (strcmp(list->name, name) == 0)
      return list->id;

    list = list->next;
  }
  return -1;
}

static int stream(PSERVER_DATA server, PCONFIGURATION config, enum platform system) {
  int appId = get_app_id(server, config->app);
  if (appId<0) {
    fprintf(stderr, "Can't find app %s\n", config->app);
    sprintf(message_buffer, "Can't find app %s\n", config->app);
    is_error = 1;
    return -1;
  }

  int gamepads = n3ds_input_num_controllers();
  int gamepad_mask = 0;
  for (int i = 0; i < gamepads && i < 4; i++)
    gamepad_mask = (gamepad_mask << 1) + 1;

  int ret = gs_start_app(server, &config->stream, appId, config->sops, config->localaudio, gamepad_mask);
  if (ret < 0) {
    if (ret == GS_NOT_SUPPORTED_4K)
      fprintf(stderr, "Server doesn't support 4K\n");
    else if (ret == GS_NOT_SUPPORTED_MODE)
      fprintf(stderr, "Server doesn't support %dx%d (%d fps) or remove --nounsupported option\n", config->stream.width, config->stream.height, config->stream.fps);
    else if (ret == GS_NOT_SUPPORTED_SOPS_RESOLUTION)
      fprintf(stderr, "Optimal Playable Settings isn't supported for the resolution %dx%d, use supported resolution or add --nosops option\n", config->stream.width, config->stream.height);
    else if (ret == GS_ERROR)
      fprintf(stderr, "Gamestream error: %s\n", gs_error);
    else
      fprintf(stderr, "Errorcode starting app: %d\n", ret);

    sprintf(message_buffer, "Errorcode starting app: %d\n", ret);
    is_error = 1;
    return -1;
  }


  if (config->debug_level > 0) {
    printf("Stream %d x %d, %d fps, %d kbps\n", config->stream.width, config->stream.height, config->stream.fps, config->stream.bitrate);
    connection_debug = true;
  }

  platform_start(system);
  LiStartConnection(&server->serverInfo, &config->stream, &connection_callbacks, platform_get_video(system), platform_get_audio(system, config->audio_device), NULL, 0, config->audio_device, 0);

  return 0;
}

int main(int argc, char* argv[]) {
  n3ds_proc_init();

  WHBGfxInit();
  n3ds_setup_renderstate();

#ifdef DEBUG
  Debug_Init();
  printf("Moonlight Wii U started\n");
#endif

  n3ds_input_init();

  Font_Init();

  Font_SetSize(50);
  Font_SetColor(255, 255, 255, 255);
  Font_Print(8, 58, "Reading configuration...");
  Font_Draw_TVDRC();

  CONFIGURATION config;
  config_parse(argc, argv, &config);

  // TODO
  config.unsupported = true;
  config.sops = false;

  if (config.address == NULL) {
    fprintf(stderr, "Specify an IP address\n");
    Font_Clear();
    Font_Print(8, 58, "Specify an IP address in the configuration file.\nMake sure to remove the '#' in front of the 'address' line.");
    state = STATE_INVALID;
  }
  else {
    char host_config_file[PATH_MAX];
    snprintf(host_config_file, PATH_MAX, "/vol/external01/moonlight/hosts/%s.conf", config.address);
    if (access(host_config_file, R_OK) != -1)
      config_file_parse(host_config_file, &config);

    // automatically connect on first launch
    state = STATE_CONNECTING;
  }

  n3ds_stream_init(config.stream.width, config.stream.height);

  SERVER_DATA server;
  while (n3ds_proc_running()) {
    switch (state) {
      case STATE_INVALID: {
        Font_Draw_TVDRC();
        break;
      }
      case STATE_DISCONNECTED: {
        Font_Clear();
        Font_SetSize(50);
        Font_SetColor(255, 255, 255, 255);

        Font_Printf(8, 58, "Moonlight Wii U %s (Disconnected)\n"
                           SCREEN_BAR
                           "Press \ue000 to connect to %s", VERSION_STRING, config.address);

        if (is_error) {
          Font_SetColor(255, 0, 0, 255);
        }
        else {
          Font_SetColor(0, 255, 0, 255);
        }
        Font_SetSize(50);
        Font_Print(8, 400, message_buffer);

        Font_Draw_TVDRC();

        uint32_t btns = n3ds_input_buttons_triggered();
        if (btns & VPAD_BUTTON_A) {
          message_buffer[0] = '\0';
          state = STATE_CONNECTING;
        }
        break;
      }
      case STATE_CONNECTING: {
        printf("Connecting to %s...\n", config.address);

        Font_Clear();
        Font_SetSize(50);
        Font_SetColor(255, 255, 255, 255);

        Font_Printf(8, 58, "Connecting to %s...\n", config.address);
        Font_Draw_TVDRC();

        int ret;
        if ((ret = gs_init(&server, config.address, config.port, config.key_dir, config.debug_level, config.unsupported)) == GS_OUT_OF_MEMORY) {
          fprintf(stderr, "Not enough memory\n");
          sprintf(message_buffer, "Not enough memory\n");
          is_error = 1;
          state = STATE_DISCONNECTED;
          break;
        } else if (ret == GS_ERROR) {
          fprintf(stderr, "Gamestream error: %s\n", gs_error);
          sprintf(message_buffer, "Gamestream error:\n%s\n", gs_error);
          is_error = 1;
          state = STATE_DISCONNECTED;
          break;
        } else if (ret == GS_INVALID) {
          fprintf(stderr, "Invalid data received from server: %s\n", gs_error);
          sprintf(message_buffer, "Invalid data received from server:\n%s\n", gs_error);
          is_error = 1;
          state = STATE_DISCONNECTED;
          break;
        } else if (ret == GS_UNSUPPORTED_VERSION) {
          fprintf(stderr, "Unsupported version: %s\n", gs_error);
          sprintf(message_buffer, "Unsupported version:\n%s\n", gs_error);
          is_error = 1;
          state = STATE_DISCONNECTED;
          break;
        } else if (ret != GS_OK) {
          fprintf(stderr, "Can't connect to server %s\n", config.address);
          sprintf(message_buffer, "Can't connect to server\n");
          is_error = 1;
          state = STATE_DISCONNECTED;
          break;
        }

        if (config.debug_level > 0)
          printf("NVIDIA %s, GFE %s (%s, %s)\n", server.gpuType, server.serverInfo.serverInfoGfeVersion, server.gsVersion, server.serverInfo.serverInfoAppVersion);

        if (autostream) {
          state = STATE_START_STREAM;
          break;
        }

        state = STATE_CONNECTED;
        break;
      }
      case STATE_CONNECTED: {
        Font_Clear();
        Font_SetSize(50);
        Font_SetColor(255, 255, 255, 255);

        Font_Printf(8, 58, "Moonlight Wii U %s (Connected to %s)\n"
                           SCREEN_BAR
                           "Press \ue000 to stream\nPress \ue001 to pair\n", VERSION_STRING, config.address);

        if (is_error) {
          Font_SetColor(255, 0, 0, 255);
        }
        else {
          Font_SetColor(0, 255, 0, 255);
        }
        Font_SetSize(50);
        Font_Print(8, 400, message_buffer);
        Font_Draw_TVDRC();

        uint32_t btns = n3ds_input_buttons_triggered();
        if (btns & VPAD_BUTTON_A) {
          message_buffer[0] = '\0';
          state = STATE_START_STREAM;
        } else if (btns & VPAD_BUTTON_B) {
          message_buffer[0] = '\0';
          state = STATE_PAIRING;
        }
        break;
      }
      case STATE_PAIRING: {
        char pin[5];
        sprintf(pin, "%d%d%d%d", (int)random() % 10, (int)random() % 10, (int)random() % 10, (int)random() % 10);
        printf("Please enter the following PIN on the target PC: %s\n", pin);
        Font_Clear();
        Font_SetSize(50);
        Font_SetColor(255, 255, 255, 255);
        Font_Printf(8, 58, "Please enter the following PIN on the target PC:\n%s\n", pin);
        Font_Draw_TVDRC();
        if (gs_pair(&server, &pin[0]) != GS_OK) {
          fprintf(stderr, "Failed to pair to server: %s\n", gs_error);
          sprintf(message_buffer, "Failed to pair to server:\n%s\n", gs_error);
          is_error = 1;
        } else {
          printf("Succesfully paired\n");
          sprintf(message_buffer, "Succesfully paired\n");
          is_error = 0;
        }

        // wrong state, make sure we reconnect first
        if (server.currentGame != 0) {
          state = STATE_DISCONNECTED;
          break;
        }

        state = STATE_CONNECTED;
        break;
      }
      case STATE_START_STREAM: {
        Font_Clear();
        Font_SetSize(50);
        Font_SetColor(255, 255, 255, 255);

        Font_Print(8, 58, "Starting stream...");
        Font_Draw_TVDRC();

        if (server.paired) {
          enum platform system = N3DS;
          config.stream.supportsHevc = config.codec != CODEC_H264 && (config.codec == CODEC_HEVC || platform_supports_hevc(system));

          if (stream(&server, &config, system) == 0) {
            n3ds_proc_set_home_enabled(0);
            start_input_thread();
            state = STATE_STREAMING;
            break;
          }
        }
        else {
          printf("You must pair with the PC first\n");
          sprintf(message_buffer, "You must pair with the PC first\n");
          is_error = 1;
        }

        state = STATE_CONNECTED;
        break;
      }
      case STATE_STREAMING: {
        n3ds_stream_draw();
        break;
      }
      case STATE_STOP_STREAM: {
        stop_input_thread();
        LiStopConnection();

        if (config.quitappafter) {
          if (config.debug_level > 0)
            printf("Sending app quit request ...\n");
          gs_quit_app(&server);
        }

        platform_stop(N3DS);

        n3ds_proc_set_home_enabled(1);
        state = STATE_DISCONNECTED;
        break;
      }
    }
  }

  Font_Deinit();

  n3ds_stream_fini();

  WHBGfxShutdown();

  n3ds_proc_shutdown();

  return 0;
}
