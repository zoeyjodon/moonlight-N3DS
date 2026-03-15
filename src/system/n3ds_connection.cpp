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

void N3dsConnectionListener::connection_terminated(int errorCode) {
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

    if (enable_motion) {
        HIDUSER_DisableAccelerometer();
        HIDUSER_DisableGyroscope();
    }
    connection_closed.store(true);
}

void N3dsConnectionListener::connection_log_message(const char *format,
                                                    va_list arglist) {
    if (!debug.load()) {
        return;
    }
    vprintf(format, arglist);
}

void N3dsConnectionListener::connection_status_update(int status) {
    if (!debug.load()) {
        return;
    }

    switch (status) {
    case CONN_STATUS_OKAY:
        printf("Connection is okay\n");
        break;
    case CONN_STATUS_POOR:
        printf("Connection is poor\n");
        break;
    }
}

void N3dsConnectionListener::set_motion_event_state(
    unsigned short controllerNumber, unsigned char motionType,
    unsigned short reportRateHz) {
    if (!enable_motion) {
        return;
    }

    switch (motionType) {
    case LI_MOTION_TYPE_ACCEL:
        if (reportRateHz > 0) {
            HIDUSER_EnableAccelerometer();
            // Alert the input handler
            auto pDispatcher = MessageDispatcher::get_instance();
            auto msg = std::make_shared<GenericEventMsg>(ENABLE_ACCEL);
            pDispatcher->post(msg);
        } else {
            HIDUSER_DisableAccelerometer();
        }
        break;
    case LI_MOTION_TYPE_GYRO:
        if (reportRateHz > 0) {
            HIDUSER_EnableGyroscope();
            // Alert the input handler
            auto pDispatcher = MessageDispatcher::get_instance();
            auto msg = std::make_shared<GenericEventMsg>(ENABLE_GYRO);
            pDispatcher->post(msg);
        } else {
            HIDUSER_DisableGyroscope();
        }
        break;
    }
}

N3dsConnectionListener::N3dsConnectionListener(bool enable_motion)
    : enable_motion(enable_motion) {
    MessageDispatcher::get_instance()->subscribe(
        MessageType::TOUCH_STATE_CHANGED, this);
    MessageDispatcher::get_instance()->subscribe(MessageType::EXIT_STREAM,
                                                 this);
}

N3dsConnectionListener::~N3dsConnectionListener() {
    MessageDispatcher::get_instance()->unsubscribe(
        MessageType::TOUCH_STATE_CHANGED, this);
    MessageDispatcher::get_instance()->unsubscribe(MessageType::EXIT_STREAM,
                                                   this);
}

void N3dsConnectionListener::accept(IMessage *msg) {
    switch (msg->getMessageType()) {
    case MessageType::TOUCH_STATE_CHANGED: {
        auto ttype = static_cast<TouchStateChangedMsg *>(msg)->ttype;
        debug.store(ttype == N3dsTouchType::DEBUG_TOUCH);
    } break;
    case MessageType::EXIT_STREAM: {
        connection_terminated(ML_ERROR_GRACEFUL_TERMINATION);
    } break;
    default:
        break;
    }
}

bool N3dsConnectionListener::is_connection_closed() {
    return connection_closed.load();
};

static void local_connection_terminated(int errorCode) {
    auto instance = N3dsConnectionListener::get_instance();
    if (instance != nullptr) {
        instance->connection_terminated(errorCode);
    }
}

static void local_connection_log_message(const char *format, ...) {
    auto instance = N3dsConnectionListener::get_instance();
    if (instance != nullptr) {
        va_list arglist;
        va_start(arglist, format);
        instance->connection_log_message(format, arglist);
        va_end(arglist);
    }
}

static void local_connection_status_update(int status) {
    auto instance = N3dsConnectionListener::get_instance();
    if (instance != nullptr) {
        instance->connection_status_update(status);
    }
}

static void local_set_motion_event_state(unsigned short controllerNumber,
                                         unsigned char motionType,
                                         unsigned short reportRateHz) {
    auto instance = N3dsConnectionListener::get_instance();
    if (instance != nullptr) {
        instance->set_motion_event_state(controllerNumber, motionType,
                                         reportRateHz);
    }
}

CONNECTION_LISTENER_CALLBACKS n3ds_connection_callbacks{
    .connectionTerminated = local_connection_terminated,
    .logMessage = local_connection_log_message,
    .connectionStatusUpdate = local_connection_status_update,
    .setMotionEventState = local_set_motion_event_state,
};