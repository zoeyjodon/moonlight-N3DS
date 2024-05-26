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

#include "keycode_map.hpp"
#include <3ds.h>
#include <memory>

enum N3dsTouchType {
    DISABLED,
    GAMEPAD,
    MOUSEPAD,
    KEYBOARD,
    ABSOLUTE_TOUCH,
    DS_TOUCH
};
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
    int previous_x = 0;
    int previous_y = 0;
};

enum KeyState { KEY_DISABLED, KEY_TEMPORARY, KEY_LOCKED, KEY_SHIFT };
struct KeyInfo {
    KeyState state = KEY_DISABLED;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
};

class KeyboardTouchHandler : public TouchHandlerBase {
  public:
    KeyboardTouchHandler();

  private:
    void _handle_touch_down(touchPosition touch);
    void _handle_touch_up(touchPosition touch);
    void _handle_touch_hold(touchPosition touch);

    keycode_info get_keycode(touchPosition touch);
    void set_screen(const uint8_t *bgr_buffer, int bgr_size);
    void set_screen_key(KeyInfo &key_info);
    void set_shift_keys();
    void handle_default();
    void cycle_key_state(KeyInfo &key_info);
    void handle_alt_keyboard();
    int get_key_mod();

  private:
    keycode_info active_keycode{-1, false};
    KeyInfo shift_info = {KEY_DISABLED, 0, 48, 136, 169};
    KeyInfo ctrl_info = {KEY_DISABLED, 64, 127, 0, 37};
    KeyInfo alt_info = {KEY_DISABLED, 128, 192, 0, 37};
    KeyInfo shift_keys = {KEY_SHIFT, 0, GSP_SCREEN_HEIGHT_BOTTOM, 37, 70};
    bool alt_keyboard_active = false;
    std::map<int, keycode_info> *selected_keycodes = nullptr;
};

class AbsoluteTouchHandler : public TouchHandlerBase {
  public:
    AbsoluteTouchHandler(int y_offset_in, int y_scale_in)
        : y_offset(y_offset_in), y_scale(y_scale_in){};

  private:
    void _handle_touch_down(touchPosition touch);
    void _handle_touch_up(touchPosition touch);
    void _handle_touch_hold(touchPosition touch);

  private:
    int y_offset = 0;
    int y_scale = 1;
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
    inline void n3dsinput_set_touch(enum N3dsTouchType ttype);
    inline bool next_touchpad_pressed(touchPosition touch);
    inline bool previous_touchpad_pressed(touchPosition touch);

  private:
    GAMEPAD_STATE *gamepad_state;
    N3dsTouchType touch_type;
    std::unique_ptr<TouchHandlerBase> handler = nullptr;
};
