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

#include "config.h"
#include "util.h"

#include "audio/audio.h"
#include "n3ds/pair_record.hpp"

#include <getopt.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

extern ssize_t getline(char **buf, size_t *bufsiz, FILE *fp);

#define MOONLIGHT_PATH "/moonlight"
#define USER_PATHS "."
#define DEFAULT_CONFIG_DIR "/.config"
#define DEFAULT_CACHE_DIR "/.cache"

#define write_config_string(fd, key, value) fprintf(fd, "%s = %s\n", key, value)
#define write_config_int(fd, key, value) fprintf(fd, "%s = %d\n", key, value)
#define write_config_bool(fd, key, value)                                      \
    fprintf(fd, "%s = %s\n", key, value ? "true" : "false")

static struct option long_options[] = {
    {"720", no_argument, NULL, 'a'},
    {"1080", no_argument, NULL, 'b'},
    {"4k", no_argument, NULL, '0'},
    {"width", required_argument, NULL, 'c'},
    {"height", required_argument, NULL, 'd'},
    {"bitrate", required_argument, NULL, 'g'},
    {"packetsize", required_argument, NULL, 'h'},
    {"app", required_argument, NULL, 'i'},
    {"mapping", required_argument, NULL, 'k'},
    {"sops", required_argument, NULL, 'l'},
    {"audio", required_argument, NULL, 'm'},
    {"localaudio", required_argument, NULL, 'n'},
    {"config", required_argument, NULL, 'o'},
    {"platform", required_argument, NULL, 'p'},
    {"save", required_argument, NULL, 'q'},
    {"keydir", required_argument, NULL, 'r'},
    {"remote", required_argument, NULL, 's'},
    {"windowed", no_argument, NULL, 't'},
    {"surround", required_argument, NULL, 'u'},
    {"fps", required_argument, NULL, 'v'},
    {"nounsupported", no_argument, NULL, 'y'},
    {"quitappafter", required_argument, NULL, '1'},
    {"viewonly", required_argument, NULL, '2'},
    {"verbose", no_argument, NULL, 'z'},
    {"debug", required_argument, NULL, 'Z'},
    {"nomouseemulation", no_argument, NULL, '4'},
    {"pin", required_argument, NULL, '5'},
    {"port", required_argument, NULL, '6'},
    {"hdr", no_argument, NULL, '7'},
    {"hwdecode", required_argument, NULL, '8'},
    {"display_type", required_argument, NULL, '9'},
    {"motion_controls", required_argument, NULL, 'e'},
    {"swapfacebuttons", required_argument, NULL, 'A'},
    {"swaptriggersandshoulders", required_argument, NULL, 'B'},
    {"usetriggersformouse", required_argument, NULL, 'C'},
    {0, 0, 0, 0},
};

void parse_argument(int c, char *value, PCONFIGURATION config) {
    switch (c) {
    case 'a':
        config->stream.width = 1280;
        config->stream.height = 720;
        break;
    case 'b':
        config->stream.width = 1920;
        config->stream.height = 1080;
        break;
    case '0':
        config->stream.width = 3840;
        config->stream.height = 2160;
        break;
    case 'c':
        config->stream.width = atoi(value);
        break;
    case 'd':
        config->stream.height = atoi(value);
        break;
    case 'e':
        config->motion_controls =
            ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case 'g':
        config->stream.bitrate = atoi(value);
        break;
    case 'h':
        config->stream.packetSize = atoi(value);
        break;
    case 'i':
        config->app = value;
        break;
    case 'l':
        config->sops = ((value == NULL) || (strcmp(value, "false") != 0));
        break;
    case 'm':
        config->audio_device = value;
        break;
    case 'n':
        config->localaudio = ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case 'o':
        if (!config_file_parse(value, config))
            exit(EXIT_FAILURE);

        break;
    case 'p':
        config->platform = value;
        break;
    case 'q':
        config->config_file = value;
        break;
    case 'r':
        strcpy(config->key_dir, value);
        break;
    case 's':
        if (strcasecmp(value, "auto") == 0)
            config->stream.streamingRemotely = STREAM_CFG_AUTO;
        else if (strcasecmp(value, "true") == 0 ||
                 strcasecmp(value, "yes") == 0)
            config->stream.streamingRemotely = STREAM_CFG_REMOTE;
        else if (strcasecmp(value, "false") == 0 ||
                 strcasecmp(value, "no") == 0)
            config->stream.streamingRemotely = STREAM_CFG_LOCAL;
        break;
    case 'u':
        if (strcasecmp(value, "5.1") == 0)
            config->stream.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
        else if (strcasecmp(value, "7.1") == 0)
            config->stream.audioConfiguration = AUDIO_CONFIGURATION_71_SURROUND;
        break;
    case 'v':
        config->stream.fps = atoi(value);
        break;
    case 'y':
        config->unsupported = false;
        break;
    case '1':
        config->quitappafter =
            ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case '2':
        config->viewonly = ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case 'z':
        config->debug_level = 1;
        break;
    case 'Z':
        if ((value != NULL) && (strcmp(value, "true") == 0)) {
            config->debug_level = 2;
        } else {
            config->debug_level = 0;
        }
        break;
    case '4':
        config->mouse_emulation = false;
        break;
    case '5':
        config->pin = atoi(value);
        break;
    case '6':
        config->port = atoi(value);
        break;
    case '7':
        config->hdr = true;
        break;
    case '8':
        config->hwdecode = ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case '9':
        if (value != NULL) {
            config->display_type = atoi(value);
        } else {
            config->display_type = 0;
        }
        break;
    case 'A':
        config->swap_face_buttons =
            ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case 'B':
        config->swap_triggers_and_shoulders =
            ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case 'C':
        config->use_triggers_for_mouse =
            ((value != NULL) && (strcmp(value, "true") == 0));
        break;
    case 1:
        if (config->action == NULL)
            config->action = value;
        else if (config->address == NULL)
            config->address = value;
        else {
            perror("Too many options");
            exit(-1);
        }
        break;
    }
}

bool config_file_parse(char *filename, PCONFIGURATION config) {
    FILE *fd = fopen(filename, "r");
    if (fd == NULL) {
        fprintf(stderr, "Can't open configuration file: %s\n", filename);
        return false;
    }

    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, fd) != -1) {
        char *key = NULL, *value = NULL;
        if (sscanf(line, "%ms = %m[^\n]", &key, &value) == 2) {
            if (strcmp(key, "address") == 0) {
                config->address = value;
            } else {
                for (int i = 0; long_options[i].name != NULL; i++) {
                    if (strcmp(long_options[i].name, key) == 0) {
                        if (long_options[i].has_arg == required_argument)
                            parse_argument(long_options[i].val, value, config);
                        else if (strcmp("true", value) == 0)
                            parse_argument(long_options[i].val, NULL, config);
                    }
                }
            }
        }
    }
    return true;
}

void config_save(char *filename, PCONFIGURATION config) {
    FILE *fd = fopen(filename, "w");
    if (fd == NULL) {
        fprintf(stderr, "Can't open configuration file: %s\n", filename);
        exit(EXIT_FAILURE);
    }

    write_config_int(fd, "width", config->stream.width);
    write_config_int(fd, "height", config->stream.height);
    write_config_int(fd, "fps", config->stream.fps);
    write_config_int(fd, "bitrate", config->stream.bitrate);
    write_config_int(fd, "packetsize", config->stream.packetSize);
    write_config_bool(fd, "sops", config->sops);
    write_config_bool(fd, "localaudio", config->localaudio);
    write_config_bool(fd, "quitappafter", config->quitappafter);
    write_config_bool(fd, "viewonly", config->viewonly);
    write_config_bool(fd, "hwdecode", config->hwdecode);
    write_config_bool(fd, "swapfacebuttons", config->swap_face_buttons);
    write_config_bool(fd, "swaptriggersandshoulders",
                      config->swap_triggers_and_shoulders);
    write_config_bool(fd, "usetriggersformouse",
                      config->use_triggers_for_mouse);
    write_config_bool(fd, "debug", config->debug_level);
    write_config_int(fd, "display_type", config->display_type);
    write_config_bool(fd, "motion_controls", config->motion_controls);

    if (strcmp(config->app, "Steam") != 0)
        write_config_string(fd, "app", config->app);

    fclose(fd);
}

void config_parse(int argc, char *argv[], PCONFIGURATION config) {
    LiInitializeStreamConfiguration(&config->stream);

    config->stream.width = 1280;
    config->stream.height = 720;
    config->stream.fps = 60;
    config->stream.bitrate = -1;
    config->stream.packetSize = 1392;
    config->stream.streamingRemotely = STREAM_CFG_AUTO;
    config->stream.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
    config->stream.supportedVideoFormats = SCM_H264;

    config->debug_level = 0;
    config->platform = "auto";
    config->app = "Steam";
    config->action = NULL;
    config->address = NULL;
    config->config_file = NULL;
    config->audio_device = NULL;
    config->sops = true;
    config->localaudio = false;
    config->unsupported = true;
    config->quitappafter = false;
    config->viewonly = false;
    config->mouse_emulation = true;
    config->hdr = false;
    config->pin = 0;
    config->port = 47989;

    config->inputsCount = 0;
    config->mapping = (char *)"";
    strcpy(config->key_dir, MOONLIGHT_3DS_PATH "/keys");

    config->stream.width = 800;
    config->stream.height = 480;
    config->stream.fps = 60;
    config->stream.encryptionFlags = ENCFLG_NONE;
    config->hwdecode = true;
    config->display_type = 0;
    config->motion_controls = false;
    config->swap_face_buttons = false;
    config->swap_triggers_and_shoulders = false;
    config->use_triggers_for_mouse = false;

    char *config_file = (char *)MOONLIGHT_3DS_PATH "/moonlight.conf";
    if (config_file)
        config_file_parse(config_file, config);

    if (config->stream.bitrate == -1) {
        // This table prefers 16:10 resolutions because they are
        // only slightly more pixels than the 16:9 equivalents, so
        // we don't want to bump those 16:10 resolutions up to the
        // next 16:9 slot.

        if (config->stream.width * config->stream.height <= 640 * 360) {
            config->stream.bitrate = (int)(1000 * (config->stream.fps / 30.0));
        } else if (config->stream.width * config->stream.height <= 854 * 480) {
            config->stream.bitrate = (int)(1500 * (config->stream.fps / 30.0));
        } else if (config->stream.width * config->stream.height <= 1366 * 768) {
            // This covers 1280x720 and 1280x800 too
            config->stream.bitrate = (int)(5000 * (config->stream.fps / 30.0));
        } else if (config->stream.width * config->stream.height <=
                   1920 * 1200) {
            config->stream.bitrate = (int)(10000 * (config->stream.fps / 30.0));
        } else if (config->stream.width * config->stream.height <=
                   2560 * 1600) {
            config->stream.bitrate = (int)(20000 * (config->stream.fps / 30.0));
        } else /* if (config->stream.width * config->stream.height <= 3840 *
                  2160) */
        {
            config->stream.bitrate = (int)(40000 * (config->stream.fps / 30.0));
        }
    }
}
