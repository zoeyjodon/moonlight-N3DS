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

#include "../system/subscriber.hpp"
#include "n3ds/N3dsTouchscreenInput.hpp"
#include <stdbool.h>

class N3dsInput : public ISubscriber {
  public:
    N3dsInput(N3dsTouchType touch_type, bool swap_face_buttons,
              bool swap_triggers_and_shoulders, bool use_triggers_for_mouse_in);
    ~N3dsInput();
    void accept(IMessage *msg) override;
    int n3dsinput_handle_event();

  private:
    void _add_gamepad();
    void _remove_gamepad();
    int _n3ds_to_li_buttons(u32 key_n3ds);
    bool _gamepad_state_changed();
    bool _accelerometer_state_changed();
    bool _gyroscope_state_changed();

  private:
    GAMEPAD_STATE gamepad_state, previous_state;
    std::unique_ptr<N3dsTouchscreenInput> touch_handler = nullptr;

    float gyro_coeff = 0;
    // Note: This was found experimentally and may need a calibration option in
    // settings
    float accel_coeff = 52.0;
    bool enable_gyro = false;
    bool enable_accel = false;
    bool use_triggers_for_mouse = false;

    uint32_t CUSTOM_KEY_A;
    uint32_t CUSTOM_KEY_B;
    uint32_t CUSTOM_KEY_X;
    uint32_t CUSTOM_KEY_Y;
    uint32_t CUSTOM_KEY_L;
    uint32_t CUSTOM_KEY_R;
    uint32_t CUSTOM_KEY_ZL;
    uint32_t CUSTOM_KEY_ZR;
};
