#pragma once

#include <3ds.h>

enum MessageType {
    TOUCHSCREEN_EVENT,
    ENABLE_ACCEL,
    ENABLE_GYRO,
    TOUCH_STATE_CHANGED,
    KEYBOARD_STATE_CHANGED,
    EXIT_STREAM,
    MESSAGE_TYPE_COUNT
};

class IMessage {
  public:
    virtual ~IMessage() = default;
    virtual MessageType getMessageType() = 0;
};

enum TouchscreenEventMsgType { DOWN, HOLD, UP };
class TouchscreenEventMsg : public IMessage {
  public:
    TouchscreenEventMsg(TouchscreenEventMsgType event_in,
                        touchPosition touch_in)
        : event(event_in), touch(touch_in){};
    ~TouchscreenEventMsg() = default;
    MessageType getMessageType() override {
        return MessageType::TOUCHSCREEN_EVENT;
    };
    TouchscreenEventMsgType event;
    touchPosition touch;
};

enum class N3dsTouchType {
    DISABLED,
    GAMEPAD,
    MOUSEPAD,
    KEYBOARD,
    ABSOLUTE_TOUCH,
    DS_TOUCH,
    MAGNIFY_TOUCH,
    MENU_TOUCH,
    DEBUG_TOUCH,
};
class TouchStateChangedMsg : public IMessage {
  public:
    TouchStateChangedMsg(N3dsTouchType ttype_in) : ttype(ttype_in){};
    ~TouchStateChangedMsg() = default;
    MessageType getMessageType() override {
        return MessageType::TOUCH_STATE_CHANGED;
    };
    N3dsTouchType ttype;
};

class KeyboardStateChangedMsg : public IMessage {
  public:
    KeyboardStateChangedMsg(const uint8_t *keyboard_image_in, int key_offset_in,
                            int key_size_in)
        : keyboard_image(keyboard_image_in), key_offset(key_offset_in),
          key_size(key_size_in){};
    ~KeyboardStateChangedMsg() = default;
    MessageType getMessageType() override {
        return MessageType::KEYBOARD_STATE_CHANGED;
    };
    const uint8_t *keyboard_image;
    int key_offset;
    int key_size;
};

class GenericEventMsg : public IMessage {
  public:
    GenericEventMsg(MessageType event_in) : event(event_in){};
    ~GenericEventMsg() = default;
    MessageType getMessageType() override { return event; };
    MessageType event;
};
