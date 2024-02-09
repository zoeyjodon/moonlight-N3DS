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
#include "platform_main.h"
#include "config.h"

#include "n3ds/n3ds_connection.h"
#include "n3ds/pair_record.h"

#include "audio/audio.h"
#include "video/video.h"

#include "input/n3ds_input.h"

#include <3ds.h>

#include <Limelight.h>

#include <client.h>
#include <discover.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <openssl/rand.h>

#define SOC_ALIGN       0x1000
// 0x40000 for each enet host (2 hosts total)
// 0x40000 for each platform socket (2 sockets total)
#define SOC_BUFFERSIZE  0x100000

#define MAX_INPUT_CHAR 60
#define MAX_APP_LIST 30

static u32 *SOC_buffer = NULL;

static PrintConsole topScreen;
static PrintConsole bottomScreen;

static inline void wait_for_button(char* prompt) {
  if (prompt == NULL) {
    printf("\nPress any button to continue\n");
  }
  else {
    printf("\n%s\n", prompt);
  }
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
}

static void n3ds_exit_handler(void)
{
  // Allow users to decide when to exit
  wait_for_button("Press any button to quit");

  NDMU_UnlockState();
  NDMU_LeaveExclusiveState();
  ndmuExit();
  irrstExit();
  SOCU_ShutdownSockets();
  SOCU_CloseSockets();
  socExit();
  free(SOC_buffer);
  romfsExit();
  aptExit();
  gfxExit();
  acExit();
}

static int console_selection_prompt(char* prompt, char** options, int option_count, int default_idx)
{
  int option_idx = default_idx;
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
      printf("Press A to confirm\n");
      printf("Press B to go back\n\n");

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
    if (kDown & KEY_B) {
      consoleClear();
      return -1;
    }
    if (kDown & KEY_DOWN) {
      if (option_idx < option_count - 1) {
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

static char * prompt_for_action(PSERVER_DATA server)
{
  if (server->paired) {
    char* actions[] = {
      "stream",
      "quit stream",
      "stream settings",
      "unpair",
    };
    int actions_len = sizeof(actions) / sizeof(actions[0]);
    int idx = console_selection_prompt("Select an action", actions, actions_len, 0);
    if (idx < 0) {
      return NULL;
    }
    return actions[idx];
  }
  char* actions[] = {"pair"};
  int actions_len = sizeof(actions) / sizeof(actions[0]);
  int idx = console_selection_prompt("Select an action", actions, actions_len, 0);
  if (idx < 0) {
    return NULL;
  }
  return actions[idx];
}

static char * prompt_for_address()
{
  char* address_list[MAX_PAIRED_DEVICES + 1];
  int address_count = 0;
  list_paired_addresses(address_list, &address_count);

  address_list[address_count] = "new";
  int idx = console_selection_prompt("Select a server address", address_list, address_count + 1, 0);
  if (idx < 0) {
    return NULL;
  }
  else if (strcmp(address_list[idx], "new") != 0) {
    return address_list[idx];
  }

  // Prompt users for a custom address
  SwkbdState swkbd;
  char* addr_buff = malloc(MAX_INPUT_CHAR);
  swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 3, -1);
  swkbdSetHintText(&swkbd, "Address of host to connect to");
  swkbdInputText(&swkbd, addr_buff, MAX_INPUT_CHAR);
  addr_buff = realloc(addr_buff, strlen(addr_buff) + 1);
  return addr_buff;
}

static inline char * prompt_for_boolean(char* prompt, bool default_val)
{
  char* options[] = {
    "true",
    "false",
  };
  int options_len = sizeof(options) / sizeof(options[0]);
  int idx = console_selection_prompt(prompt, options, options_len, default_val ? 0 : 1);
  if (idx < 0) {
    idx = default_val ? 0 : 1;
  }
  return options[idx];
}

static void prompt_for_stream_settings(PCONFIGURATION config)
{
  char* setting_names[] = {
    "width",
    "height",
    "fps",
    "bitrate",
    "packetsize",
    "nosops",
    "localaudio",
    "quitappafter",
    "viewonly",
    "hwdecode",
    "debug",
  };
  char argument_ids[] = {
    'c',
    'd',
    'v',
    'g',
    'h',
    'l',
    'n',
    '1',
    '2',
    '8',
    'Z',
  };
  int settings_len = sizeof(argument_ids);
  char* setting_buff = malloc(MAX_INPUT_CHAR);
  char* prompt_buff = malloc(200);
  int idx = 0;
  while (1) {
    sprintf(prompt_buff, "Select a setting");
    if (config->stream.width != GSP_SCREEN_HEIGHT_TOP && \
        config->stream.width != GSP_SCREEN_HEIGHT_TOP_2X) {
      strcat(prompt_buff, "\n\nWARNING: Using an unsupported width may cause issues (3DS supports 400 or 800)\n");
    }
    if (config->stream.height != GSP_SCREEN_WIDTH) {
      strcat(prompt_buff, "\n\nWARNING: Using an unsupported height may cause issues (3DS supports 240)\n");
    }
    idx = console_selection_prompt(prompt_buff, setting_names, settings_len, idx);
    if (idx < 0) {
      break;
    }

    SwkbdState swkbd;
    memset(setting_buff, 0, MAX_INPUT_CHAR);
    if (strcmp("width", setting_names[idx]) == 0) {
      swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 1, 8);
      sprintf(setting_buff, "%d", config->stream.width);
      swkbdSetInitialText(&swkbd, setting_buff);
      swkbdInputText(&swkbd, setting_buff, MAX_INPUT_CHAR);
    }
    else if (strcmp("height", setting_names[idx]) == 0) {
      swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 1, 8);
      sprintf(setting_buff, "%d", config->stream.height);
      swkbdSetInitialText(&swkbd, setting_buff);
      swkbdInputText(&swkbd, setting_buff, MAX_INPUT_CHAR);
    }
    else if (strcmp("fps", setting_names[idx]) == 0) {
      swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 1, 8);
      sprintf(setting_buff, "%d", config->stream.fps);
      swkbdSetInitialText(&swkbd, setting_buff);
      swkbdInputText(&swkbd, setting_buff, MAX_INPUT_CHAR);
    }
    else if (strcmp("bitrate", setting_names[idx]) == 0) {
      swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 1, 8);
      sprintf(setting_buff, "%d", config->stream.bitrate);
      swkbdSetInitialText(&swkbd, setting_buff);
      swkbdInputText(&swkbd, setting_buff, MAX_INPUT_CHAR);
    }
    else if (strcmp("packetsize", setting_names[idx]) == 0) {
      swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 1, 8);
      sprintf(setting_buff, "%d", config->stream.packetSize);
      swkbdSetInitialText(&swkbd, setting_buff);
      swkbdInputText(&swkbd, setting_buff, MAX_INPUT_CHAR);
    }
    else if (strcmp("nosops", setting_names[idx]) == 0) {
      char* bool_str = prompt_for_boolean("Disable sops", !config->sops);
      if (bool_str != NULL) {
        sprintf(setting_buff, bool_str);
      }
    }
    else if (strcmp("localaudio", setting_names[idx]) == 0) {
      char* bool_str = prompt_for_boolean("Enable local audio", config->localaudio);
      if (bool_str != NULL) {
        sprintf(setting_buff, bool_str);
      }
    }
    else if (strcmp("quitappafter", setting_names[idx]) == 0) {
      char* bool_str = prompt_for_boolean("Quit app after streaming", config->quitappafter);
      if (bool_str != NULL) {
        sprintf(setting_buff, bool_str);
      }
    }
    else if (strcmp("viewonly", setting_names[idx]) == 0) {
      char* bool_str = prompt_for_boolean("Disable controller input", config->viewonly);
      if (bool_str != NULL) {
        sprintf(setting_buff, bool_str);
      }
    }
    else if (strcmp("hwdecode", setting_names[idx]) == 0) {
      char* bool_str = prompt_for_boolean("Use hardware video decoder", config->hwdecode);
      if (bool_str != NULL) {
        sprintf(setting_buff, bool_str);
      }
    }
    else if (strcmp("debug", setting_names[idx]) == 0) {
      char* bool_str = prompt_for_boolean("Enable debug logs", config->debug_level);
      if (bool_str != NULL) {
        sprintf(setting_buff, bool_str);
      }
    }

    parse_argument(argument_ids[idx], setting_buff, config);
  }

  // Update the config file
  char* config_file_path = (char*) MOONLIGHT_3DS_PATH "/moonlight.conf";
  config_save(config_file_path, config);
  free(setting_buff);
}

static void init_3ds()
{
  Result status = 0;
  acInit();
  gfxInit(GSP_RGB565_OES, GSP_BGR8_OES, false);
  consoleInit(GFX_TOP, &topScreen);
  consoleSelect(&topScreen);
  atexit(n3ds_exit_handler);

  osSetSpeedupEnable(true);
  aptSetSleepAllowed(false);
  aptInit();

  SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);
  status = socInit(SOC_buffer, SOC_BUFFERSIZE);
  if (R_FAILED(status))
  {
    printf("socInit: %08lX\n", status);
    exit(1);
  }

  status = ndmuInit();
  status |= NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE);
  status |= NDMU_LockState();
  if (R_FAILED(status))
  {
    printf ("Warning: failed to enter exclusive NDM state: %08lX\n", status);
    wait_for_button(NULL);
  }
}

static int prompt_for_app_id(PSERVER_DATA server)
{
  PAPP_LIST list = NULL;
  if (gs_applist(server, &list) != GS_OK) {
    fprintf(stderr, "Can't get app list\n");
    return -1;
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

  int id_idx = console_selection_prompt("Select an app", app_names, idx, 0);
  if (id_idx == -1) {
    return -1;
  }
  return app_ids[id_idx];
}

static inline void stream_loop(PCONFIGURATION config) {
  bool done = false;
  while(!done && aptMainLoop()) {
    done = n3ds_connection_closed;
    if (!config->viewonly) {
      done |= n3dsinput_handle_event();
    }
    hidWaitForEvent(HIDEVENT_PAD0, true);
  }
}

static void stream(PSERVER_DATA server, PCONFIGURATION config, int appId) {
  int gamepad_mask = 1;
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

  n3ds_audio_disabled = config->localaudio;
  n3ds_connection_debug = config->debug_level;

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

  PDECODER_RENDERER_CALLBACKS video_callbacks = config->hwdecode ? &decoder_callbacks_n3ds_mvd : &decoder_callbacks_n3ds;

  printf("Loading...\nStream %dx%d, %dfps, %dkbps, sops=%d, localaudio=%d, quitappafter=%d,\
 viewonly=%d, rotate=%d, encryption=%x, hwdecode=%d, debug=%d\n",
          config->stream.width,
          config->stream.height,
          config->stream.fps,
          config->stream.bitrate,
          config->sops,
          config->localaudio,
          config->quitappafter,
          config->viewonly,
          config->rotate,
          config->stream.encryptionFlags,
          config->hwdecode,
          config->debug_level
        );

  int status = LiStartConnection(&server->serverInfo, &config->stream, &n3ds_connection_callbacks, video_callbacks, &audio_callbacks_n3ds, NULL, drFlags, config->audio_device, 0);

  if (status != 0) {
    n3ds_connection_callbacks.connectionTerminated(status);
    exit(status);
  }

  consoleClear();
  if (config->debug_level) {
    consoleInit(GFX_BOTTOM, &bottomScreen);
    consoleSelect(&bottomScreen);
    printf("Connected!\n");
  }
  else {
    n3dsinput_set_touch(GAMEPAD);
  }

  stream_loop(config);

  LiStopConnection();

  if (config->quitappafter) {
    printf("Sending app quit request ...\n");
    gs_quit_app(server);
  }
}

int main(int argc, char* argv[]) {
  init_3ds();

  CONFIGURATION config;
  config_parse(argc, argv, &config);

  while (aptMainLoop()) {
    config.address = prompt_for_address();
    if (config.address == NULL) {
      continue;
    }

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
      wait_for_button(NULL);
      continue;
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

    while (aptMainLoop()) {
      config.action = prompt_for_action(&server);
      if (config.action == NULL) {
        break;
      }
      else if (strcmp("stream", config.action) == 0) {
        int appId = prompt_for_app_id(&server);
        if (appId == -1) {
          continue;
        }

        config.stream.supportedVideoFormats = VIDEO_FORMAT_H264;

        if (config.viewonly) {
          if (config.debug_level > 0)
            printf("View-only mode enabled, no input will be sent to the host computer\n");
        } else {
          n3dsinput_init();
        }
        stream(&server, &config, appId);

        if (!config.viewonly) {
          n3dsinput_cleanup();
        }
        // Exit app after streaming has closed
        exit(0);
      }
      else if (strcmp("pair", config.action) == 0) {
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
          break;
        }
      }
      else if (strcmp("stream settings", config.action) == 0) {
        prompt_for_stream_settings(&config);
        continue;
      }
      else if (strcmp("unpair", config.action) == 0) {
        if (gs_unpair(&server) != GS_OK) {
          fprintf(stderr, "Failed to unpair to server: %s\n", gs_error);
        } else {
          printf("Succesfully unpaired\n");
          remove_pair_address(config.address);
          break;
        }
      }
      else if (strcmp("quit stream", config.action) == 0) {
        printf("Sending app quit request ...\n");
        gs_quit_app(&server);
      }
      else
        fprintf(stderr, "%s is not a valid action\n", config.action);

      wait_for_button(NULL);
    }
  }
  return 0;
}

#endif
