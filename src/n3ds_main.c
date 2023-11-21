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

#ifdef __3DS__

#include "loop.h"
#include "connection_main.h"
#include "platform_main.h"
#include "config.h"
#include "sdl_main.h"

#include "n3ds/pair_record.h"

#include "audio/audio.h"
#include "video/video.h"

#include "input/sdl.h"

#include <3ds.h>

#include <Limelight.h>

#include <client.h>
#include <discover.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/rand.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x1000000

#define MAX_INPUT_CHAR 60
#define MAX_APP_LIST 30

static u32 *SOC_buffer = NULL;

PrintConsole topScreen;
PrintConsole bottomScreen;

static void n3ds_exit_handler(void)
{
  // Allow users to decide when to exit
  printf("\nPress any button to quit\n");
	while (aptMainLoop())
	{
		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		hidScanInput();
		u32 kDown = hidKeysDown();

		if (kDown)
			break;
	}

	irrstExit();
	socExit();
	free(SOC_buffer);
	romfsExit();
	aptExit();
	gfxExit();
	acExit();
}

int console_selection_prompt(char* prompt, char** options, int option_count)
{
  int option_idx = 0;
  int last_option_idx = -1;
	while (aptMainLoop())
	{
    if (option_idx != last_option_idx)
    {
      consoleClear();
      if (prompt) {
        printf("%s\n", prompt);
      }
      printf("Press up/down to select\n");
      printf("Press A to confirm\n\n");

      for (int i = 0; i < option_count; i++) {
        if (i == option_idx) {
          printf(">%s\n", options[i]);
        }
        else {
          printf("%s\n", options[i]);
        }
      }
      last_option_idx = option_idx;
    }

		gfxSwapBuffers();
		gfxFlushBuffers();
		gspWaitForVBlank();

		hidScanInput();
		u32 kDown = hidKeysDown();

		if (kDown & KEY_A) {
			consoleClear();
      return option_idx;
    }
		if (kDown & KEY_DOWN) {
			if (option_idx < 4) {
				option_idx++;
			}
		}
    else if (kDown & KEY_UP) {
			if (option_idx > 0) {
				option_idx--;
			}
		}
	}

  exit(0);
}

char * prompt_for_action(PSERVER_DATA server)
{
  if (server->paired) {
    const char* actions[4];
    actions[0] = "stream";
    actions[1] = "quit stream";
    actions[2] = "unpair";
    actions[3] = "change server";
    int idx = console_selection_prompt("Select an action", actions, 4);
    return actions[idx];
  }
  const char* actions[2];
  actions[0] = "pair";
  actions[1] = "change server";
  int idx = console_selection_prompt("Select an action", actions, 2);
  return actions[idx];
}

char * prompt_for_address()
{
  char* address_list[MAX_PAIRED_DEVICES + 1];
  int address_count = 0;
  list_paired_addresses(address_list, &address_count);

  address_list[address_count] = "new";
  int idx = console_selection_prompt("Select a server address", address_list, address_count + 1);
  if (strcmp(address_list[idx], "new") != 0) {
    return address_list[idx];
  }

  // Prompt users for a custom address
  SwkbdState swkbd;
  char* addr_buff = malloc(MAX_INPUT_CHAR);
  swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 3, -1);
  swkbdInputText(&swkbd, addr_buff, MAX_INPUT_CHAR);
  addr_buff = realloc(addr_buff, strlen(addr_buff));
  return addr_buff;
}

void init_3ds()
{
	acInit();
	gfxInitDefault();
	consoleInit(GFX_TOP, &topScreen);
	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&topScreen);
  atexit(n3ds_exit_handler);

  osSetSpeedupEnable(true);
	aptSetSleepAllowed(true);
	aptInit();
	Result status = romfsInit();
	if (R_FAILED(status))
  {
    printf("romfsInit: %08lX\n", status);
  }
	else printf("romfs Init Successful!\n");

	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
	status = socInit(SOC_buffer, SOC_BUFFERSIZE);
	if (R_FAILED(status))
	{
    printf("socInit: %08lX\n", status);
    exit(1);
	}

	aptSetSleepAllowed (false);

	status = NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE);
	if (R_FAILED(status))
  {
		printf ("Failed to enter exclusive NDM state: %08lX\n", status);
  }
  status = NDMU_LockState();
	if (R_FAILED(status))
	{
		printf ("Failed to lock NDM: %08lX\n", status);
		NDMU_LeaveExclusiveState();
	}
}

int prompt_for_app_id(PSERVER_DATA server)
{
  PAPP_LIST list = NULL;
  if (gs_applist(server, &list) != GS_OK) {
    fprintf(stderr, "Can't get app list\n");
    return;
  }

  char* app_names[MAX_APP_LIST];
  int app_ids[MAX_APP_LIST];
  int idx = 0;
  for (idx = 0; idx < MAX_APP_LIST; idx++) {
    if (list == NULL) {
      break;
    }

    printf("%d. %s\n", idx, list->name);
    app_names[idx] = malloc(strlen(list->name));
    strcpy(app_names[idx], list->name);
    app_ids[idx] = list->id;
    list = list->next;
  }

  int id_idx = console_selection_prompt("Select an app", app_names, idx);
  return app_ids[id_idx];
}

static void stream(PSERVER_DATA server, PCONFIGURATION config, enum platform system, int appId) {
  int gamepads = sdl_gamepads;
  int gamepad_mask = 0;
  for (int i = 0; i < gamepads; i++)
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
    exit(-1);
  }

  int drFlags = 0;
  if (config->fullscreen)
    drFlags |= DISPLAY_FULLSCREEN;

  switch (config->rotate) {
  case 0:
    break;
  case 90:
    drFlags |= DISPLAY_ROTATE_90;
    break;
  case 180:
    drFlags |= DISPLAY_ROTATE_180;
    break;
  case 270:
    drFlags |= DISPLAY_ROTATE_270;
    break;
  default:
    printf("Ignoring invalid rotation value: %d\n", config->rotate);
  }

  if (config->debug_level > 0) {
    printf("Stream %d x %d, %d fps, %d kbps\n", config->stream.width, config->stream.height, config->stream.fps, config->stream.bitrate);
    connection_debug = true;
  }

  int status = LiStartConnection(&server->serverInfo, &config->stream, &connection_callbacks, platform_get_video(system), platform_get_audio(system, config->audio_device), NULL, drFlags, config->audio_device, 0);
  if (status != 0) {
    exit(status);
  }

	consoleInit(GFX_BOTTOM, &bottomScreen);
	consoleSelect(&bottomScreen);
  sdl_loop();

  LiStopConnection();

  if (config->quitappafter) {
    if (config->debug_level > 0)
      printf("Sending app quit request ...\n");
    gs_quit_app(server);
  }
}

int main(int argc, char* argv[]) {
  init_3ds();

  CONFIGURATION config;
  config_parse(argc, argv, &config);

  config.address = prompt_for_address();

  while (aptMainLoop()) {
    char host_config_file[128];
    sprintf(host_config_file, "hosts/%s.conf", config.address);
    if (access(host_config_file, R_OK) != -1)
      config_file_parse(host_config_file, &config);

    SERVER_DATA server;
    printf("Connecting to %s...\n", config.address);

    int ret;
    if ((ret = gs_init(&server, config.address, config.port, config.key_dir, config.debug_level, config.unsupported)) == GS_OUT_OF_MEMORY) {
      fprintf(stderr, "Not enough memory\n");
      exit(-1);
    } else if (ret == GS_ERROR) {
      fprintf(stderr, "Gamestream error: %s\n", gs_error);
      exit(-1);
    } else if (ret == GS_INVALID) {
      fprintf(stderr, "Invalid data received from server: %s\n", gs_error);
      exit(-1);
    } else if (ret == GS_UNSUPPORTED_VERSION) {
      fprintf(stderr, "Unsupported version: %s\n", gs_error);
      exit(-1);
    } else if (ret != GS_OK) {
      fprintf(stderr, "Can't connect to server %s\n", config.address);
      exit(-1);
    }

    if (config.debug_level > 0) {
      printf("GPU: %s, GFE: %s (%s, %s)\n", server.gpuType, server.serverInfo.serverInfoGfeVersion, server.gsVersion, server.serverInfo.serverInfoAppVersion);
      printf("Server codec flags: 0x%x\n", server.serverInfo.serverCodecModeSupport);
    }
    if (server.paired) {
      add_pair_address(config.address);
    }
    else {
      remove_pair_address(config.address);
    }

    config.action = prompt_for_action(&server);
    if (strcmp("change server", config.action) == 0) {
      config.address = prompt_for_address();
      // Skip the additional prompt for a button press
      continue;
    } else if (strcmp("stream", config.action) == 0) {
      enum platform system = platform_check(config.platform);
      if (config.debug_level > 0)
        printf("Platform %s\n", platform_name(system));

      if (system == 0) {
        fprintf(stderr, "Platform '%s' not found\n", config.platform);
        exit(-1);
      } else if (system == SDL && config.audio_device != NULL) {
        fprintf(stderr, "You can't select a audio device for SDL\n");
        exit(-1);
      }

      int appId = prompt_for_app_id(&server);

      config.stream.supportedVideoFormats = VIDEO_FORMAT_H264;
      sdl_init(config.stream.width, config.stream.height, config.fullscreen);

      if (config.viewonly) {
        if (config.debug_level > 0)
          printf("View-only mode enabled, no input will be sent to the host computer\n");
      } else {
        sdlinput_init(config.mapping);
        rumble_handler = sdlinput_rumble;
        rumble_triggers_handler = sdlinput_rumble_triggers;
        set_motion_event_state_handler = sdlinput_set_motion_event_state;
        set_controller_led_handler = sdlinput_set_controller_led;
      }
      stream(&server, &config, system, appId);
      // Exit app after streaming has closed
      exit(0);
    } else if (strcmp("pair", config.action) == 0) {
      char pin[5];
      if (config.pin > 0 && config.pin <= 9999) {
        sprintf(pin, "%04d", config.pin);
      } else {
        sprintf(pin, "%d%d%d%d", (unsigned)random() % 10, (unsigned)random() % 10, (unsigned)random() % 10, (unsigned)random() % 10);
      }
      printf("Please enter the following PIN on the target PC:\n%s\n", pin);
      fflush(stdout);
      if (gs_pair(&server, &pin[0]) != GS_OK) {
        fprintf(stderr, "Failed to pair to server: %s\n", gs_error);
      } else {
        printf("Succesfully paired\n");
        add_pair_address(config.address);
      }
    } else if (strcmp("unpair", config.action) == 0) {
      if (gs_unpair(&server) != GS_OK) {
        fprintf(stderr, "Failed to unpair to server: %s\n", gs_error);
      } else {
        printf("Succesfully unpaired\n");
        remove_pair_address(config.address);
      }
    } else if (strcmp("quit stream", config.action) == 0) {
      gs_quit_app(&server);
    } else
      fprintf(stderr, "%s is not a valid action\n", config.action);


    printf("\nPress any button to continue\n");
    while (aptMainLoop())
    {
      gfxSwapBuffers();
      gfxFlushBuffers();
      gspWaitForVBlank();
      hidScanInput();
      if (hidKeysDown())
        break;
    }
  }
  return 0;
}

#endif
