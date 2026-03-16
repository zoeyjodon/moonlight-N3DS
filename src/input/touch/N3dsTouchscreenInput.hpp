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

#include "../../system/AtomicVar.hpp"
#include "../../system/subscriber.hpp"
#include "TouchHandler.hpp"
#include <3ds.h>
#include <memory>

class N3dsTouchscreenInput : public ISubscriber {
  public:
    N3dsTouchscreenInput(GAMEPAD_STATE *gamepad_in, int image_width_in,
                         int image_height_in);
    ~N3dsTouchscreenInput();

    void accept(IMessage *msg) override;

    void n3dsinput_handle_touch(u32 kDown, u32 kUp);

  private:
    void _n3dsinput_set_touch(N3dsTouchType touch_type_in);

  private:
    GAMEPAD_STATE *gamepad_state;
    int image_width, image_height;
    AtomicVar<N3dsTouchType> next_touch_type = N3dsTouchType::DISABLED;
    N3dsTouchType touch_type = N3dsTouchType::DISABLED;
    std::unique_ptr<TouchHandlerBase> handler = nullptr;
};
