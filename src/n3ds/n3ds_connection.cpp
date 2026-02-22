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

#include "n3ds_connection.hpp"
#include "../input/n3ds_input.hpp"
#include "../system/dispatcher.hpp"

#include <3ds.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

std::unique_ptr<N3dsConnectionListener> N3dsConnectionListener::instance =
    nullptr;

static void connection_terminated(int errorCode) {
    switch (errorCode) {
    case ML_ERROR_GRACEFUL_TERMINATION:
        printf("Connection has been terminated gracefully.\n");
        break;
    case ML_ERROR_NO_VIDEO_TRAFFIC:
        printf("No video received from host. Check the host PC's firewall and "
               "port forwarding rules.\n");
        break;
    case ML_ERROR_NO_VIDEO_FRAME:
        printf("Your network connection isn't performing well. Reduce your "
               "video bitrate setting or try a faster connection.\n");
        break;
    case ML_ERROR_UNEXPECTED_EARLY_TERMINATION:
        printf("The connection was unexpectedly terminated by the host due to "
               "a video capture error. Make sure no DRM-protected content is "
               "playing on the host.\n");
        break;
    case ML_ERROR_PROTECTED_CONTENT:
        printf("The connection was terminated by the host due to DRM-protected "
               "content. Close any DRM-protected content on the host and try "
               "again.\n");
        break;
    default:
        printf("Connection terminated with error: %d\n", errorCode);
        break;
    }

    HIDUSER_DisableAccelerometer();
    HIDUSER_DisableGyroscope();
    N3dsConnectionListener::get_instance()->connection_closed = true;
}

static void connection_log_message(const char *format, ...) {
    va_list arglist;
    va_start(arglist, format);
    vprintf(format, arglist);
    va_end(arglist);
}

static void connection_status_update(int status) {
    switch (status) {
    case CONN_STATUS_OKAY:
        printf("Connection is okay\n");
        break;
    case CONN_STATUS_POOR:
        printf("Connection is poor\n");
        break;
    }
}

static void set_motion_event_state(unsigned short controllerNumber,
                                   unsigned char motionType,
                                   unsigned short reportRateHz) {
    switch (motionType) {
    case LI_MOTION_TYPE_ACCEL:
        if (reportRateHz > 0) {
            HIDUSER_EnableAccelerometer();
            // Alert the input handler
            auto pDispatcher = MessageDispatcher::get_instance();
            GenericEventMsg msg(ENABLE_ACCEL);
            pDispatcher->post_immediate(&msg);
        } else {
            HIDUSER_DisableAccelerometer();
        }
        break;
    case LI_MOTION_TYPE_GYRO:
        if (reportRateHz > 0) {
            HIDUSER_EnableGyroscope();
            // Alert the input handler
            auto pDispatcher = MessageDispatcher::get_instance();
            GenericEventMsg msg(ENABLE_GYRO);
            pDispatcher->post_immediate(&msg);
        } else {
            HIDUSER_DisableGyroscope();
        }
        break;
    }
}

N3dsConnectionListener::N3dsConnectionListener(bool debug, bool enable_motion) {
    n3ds_connection_callbacks.stageStarting = NULL;
    n3ds_connection_callbacks.stageComplete = NULL;
    n3ds_connection_callbacks.stageFailed = NULL;
    n3ds_connection_callbacks.connectionStarted = NULL;
    n3ds_connection_callbacks.rumble = NULL;
    n3ds_connection_callbacks.setHdrMode = NULL;
    n3ds_connection_callbacks.rumbleTriggers = NULL;
    n3ds_connection_callbacks.setControllerLED = NULL;

    n3ds_connection_callbacks.connectionTerminated = connection_terminated;
    n3ds_connection_callbacks.logMessage =
        debug ? connection_log_message : NULL;
    n3ds_connection_callbacks.connectionStatusUpdate =
        debug ? connection_status_update : NULL;
    n3ds_connection_callbacks.setMotionEventState =
        enable_motion ? set_motion_event_state : NULL;
}

N3dsConnectionListener::~N3dsConnectionListener() {
    n3ds_connection_callbacks.connectionTerminated = NULL;
    n3ds_connection_callbacks.logMessage = NULL;
    n3ds_connection_callbacks.connectionStatusUpdate = NULL;
    n3ds_connection_callbacks.setMotionEventState = NULL;
}
