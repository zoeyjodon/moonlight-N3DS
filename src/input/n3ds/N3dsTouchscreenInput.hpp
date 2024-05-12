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

#include <3ds.h>
#include <memory>

enum N3dsTouchType { DISABLED, GAMEPAD, MOUSEPAD, DS_TOUCH };
typedef struct _GAMEPAD_STATE {
    unsigned char leftTrigger, rightTrigger;
    short leftStickX, leftStickY;
    short rightStickX, rightStickY;
    int buttons;
    float accel_vector_x, accel_vector_y, accel_vector_z;
    float gyro_rate_x, gyro_rate_y, gyro_rate_z;
} GAMEPAD_STATE;

class TouchHandlerBase {
  public:
    void handle_touch_down(touchPosition touch);
    void handle_touch_up(touchPosition touch);
    void handle_touch_hold(touchPosition touch);

  private:
    virtual void _handle_touch_down(touchPosition touch) = 0;
    virtual void _handle_touch_up(touchPosition touch) = 0;
    virtual void _handle_touch_hold(touchPosition touch) = 0;

  private:
    bool isActive = false;
};

class GamepadTouchHandler : public TouchHandlerBase {
  public:
    GamepadTouchHandler(GAMEPAD_STATE *gamepad_in);

  private:
    void _handle_touch_down(touchPosition touch);
    void _handle_touch_up(touchPosition touch);
    void _handle_touch_hold(touchPosition touch);

  private:
    GAMEPAD_STATE *gamepad_state;
};

class MouseTouchHandler : public TouchHandlerBase {
  public:
    MouseTouchHandler();

  private:
    void _handle_touch_down(touchPosition touch);
    void _handle_touch_up(touchPosition touch);
    void _handle_touch_hold(touchPosition touch);

  private:
    int mouse_button = -1;
    bool special_key = false;
    int previous_x = 0;
    int previous_y = 0;
};

class AbsoluteTouchHandler : public TouchHandlerBase {
  private:
    void _handle_touch_down(touchPosition touch);
    void _handle_touch_up(touchPosition touch);
    void _handle_touch_hold(touchPosition touch);

  private:
    int previous_x = 0;
    int previous_y = 0;
};

class N3dsTouchscreenInput {
  public:
    N3dsTouchscreenInput(GAMEPAD_STATE *gamepad_in,
                         N3dsTouchType touch_type_in);
    ~N3dsTouchscreenInput() = default;

    void n3dsinput_handle_touch(u32 kDown, u32 kUp);

  private:
    inline void init_touch_handler();
    inline void n3dsinput_cycle_touch();
    inline void n3dsinput_set_touch(enum N3dsTouchType ttype);
    inline bool change_touchpad_pressed(touchPosition touch);

  private:
    GAMEPAD_STATE *gamepad_state;
    N3dsTouchType touch_type;
    std::unique_ptr<TouchHandlerBase> handler = nullptr;
};
