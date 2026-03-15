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

#include "../system/AtomicVar.hpp"
#include "../system/subscriber.hpp"
#include <Limelight.h>
#include <memory>

class N3dsConnectionListener : public ISubscriber {
  public:
    N3dsConnectionListener(bool enable_motion);
    ~N3dsConnectionListener();

    void accept(IMessage *msg) override;

    static N3dsConnectionListener *create_instance(bool enable_motion) {
        if (instance == nullptr) {
            instance = std::make_unique<N3dsConnectionListener>(enable_motion);
        }
        return instance.get();
    }
    static N3dsConnectionListener *get_instance() {
        return instance != nullptr ? instance.get() : nullptr;
    }
    static void destroy_instance() { instance = nullptr; }

    void connection_terminated(int errorCode);
    void connection_log_message(const char *format, va_list arglist);
    void connection_status_update(int status);
    void set_motion_event_state(unsigned short controllerNumber,
                                unsigned char motionType,
                                unsigned short reportRateHz);

    bool is_connection_closed();

  private:
    static std::unique_ptr<N3dsConnectionListener> instance;
    bool enable_motion;
    AtomicVar<bool> debug = false;
    AtomicVar<bool> connection_closed = false;
};

extern CONNECTION_LISTENER_CALLBACKS n3ds_connection_callbacks;
