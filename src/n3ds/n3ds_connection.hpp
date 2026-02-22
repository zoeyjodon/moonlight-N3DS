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
#include <memory>

class N3dsConnectionListener {
  public:
    N3dsConnectionListener(bool debug, bool enable_motion);
    ~N3dsConnectionListener();

    static N3dsConnectionListener *create_instance(bool debug,
                                                   bool enable_motion) {
        if (instance == nullptr) {
            instance =
                std::make_unique<N3dsConnectionListener>(debug, enable_motion);
        }
        return instance.get();
    }
    static N3dsConnectionListener *get_instance() {
        return instance != nullptr ? instance.get() : nullptr;
    }
    static void destroy_instance() { instance = nullptr; }

  public:
    CONNECTION_LISTENER_CALLBACKS n3ds_connection_callbacks;
    bool connection_closed = false;

  private:
    static std::unique_ptr<N3dsConnectionListener> instance;
};
