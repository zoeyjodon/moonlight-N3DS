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

#ifndef __3DS__

#include <Limelight.h>

#include <pthread.h>
#include <stdbool.h>

extern CONNECTION_LISTENER_CALLBACKS connection_callbacks;
extern pthread_t main_thread_id;
extern bool connection_debug;
extern ConnListenerRumble rumble_handler;
extern ConnListenerRumbleTriggers rumble_triggers_handler;
extern ConnListenerSetMotionEventState set_motion_event_state_handler;
extern ConnListenerSetControllerLED set_controller_led_handler;

#endif
