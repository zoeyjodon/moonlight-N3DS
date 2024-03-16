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

#include "n3ds_connection.h"
#include "../input/n3ds_input.h"

#include <3ds.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>

bool n3ds_connection_closed = false;
bool n3ds_connection_debug = false;
bool n3ds_enable_motion = false;

static void connection_terminated(int errorCode) {
  switch (errorCode) {
  case ML_ERROR_GRACEFUL_TERMINATION:
    printf("Connection has been terminated gracefully.\n");
    break;
  case ML_ERROR_NO_VIDEO_TRAFFIC:
    printf("No video received from host. Check the host PC's firewall and port forwarding rules.\n");
    break;
  case ML_ERROR_NO_VIDEO_FRAME:
    printf("Your network connection isn't performing well. Reduce your video bitrate setting or try a faster connection.\n");
    break;
  case ML_ERROR_UNEXPECTED_EARLY_TERMINATION:
    printf("The connection was unexpectedly terminated by the host due to a video capture error. Make sure no DRM-protected content is playing on the host.\n");
    break;
  case ML_ERROR_PROTECTED_CONTENT:
    printf("The connection was terminated by the host due to DRM-protected content. Close any DRM-protected content on the host and try again.\n");
    break;
  default:
    printf("Connection terminated with error: %d\n", errorCode);
    break;
  }

  HIDUSER_DisableAccelerometer();
  HIDUSER_DisableGyroscope();
  n3ds_connection_closed = true;
}

static void connection_log_message(const char* format, ...) {
  if (n3ds_connection_debug) {
    va_list arglist;
    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
  }
}

static void connection_status_update(int status) {
  if (n3ds_connection_debug) {
    switch (status) {
      case CONN_STATUS_OKAY:
        printf("Connection is okay\n");
        break;
      case CONN_STATUS_POOR:
        printf("Connection is poor\n");
        break;
    }
  }
}

static void set_motion_event_state(unsigned short controllerNumber, unsigned char motionType, unsigned short reportRateHz) {
  if (!n3ds_enable_motion){
    return;
  }

  switch (motionType) {
  case LI_MOTION_TYPE_ACCEL:
    enable_accel = (reportRateHz > 0);
    if (enable_accel) {
      HIDUSER_EnableAccelerometer();
    }
    else {
      HIDUSER_DisableAccelerometer();
    }
    break;
  case LI_MOTION_TYPE_GYRO:
    enable_gyro = (reportRateHz > 0);
    if (enable_gyro) {
      HIDUSER_EnableGyroscope();
    }
    else {
      HIDUSER_DisableGyroscope();
    }
    break;
  }
}

CONNECTION_LISTENER_CALLBACKS n3ds_connection_callbacks = {
  .stageStarting = NULL,
  .stageComplete = NULL,
  .stageFailed = NULL,
  .connectionStarted = NULL,
  .connectionTerminated = connection_terminated,
  .logMessage = connection_log_message,
  .rumble = NULL,
  .connectionStatusUpdate = connection_status_update,
  .setHdrMode = NULL,
  .rumbleTriggers = NULL,
  .setMotionEventState = set_motion_event_state,
  .setControllerLED = NULL,
};
