#pragma once

#include <3ds.h>

enum MessageType {
    TOUCHSCREEN_EVENT,
    ENABLE_ACCEL,
    ENABLE_GYRO,
    MESSAGE_TYPE_COUNT
};

class IMessage {
  public:
    virtual MessageType getMessageType() = 0;
};

enum TouchscreenEventMsgType { DOWN, HOLD, UP };
class TouchscreenEventMsg : public IMessage {
  public:
    TouchscreenEventMsg(TouchscreenEventMsgType event_in,
                        touchPosition touch_in)
        : event(event_in), touch(touch_in){};
    ~TouchscreenEventMsg() = default;
    MessageType getMessageType() { return MessageType::TOUCHSCREEN_EVENT; };
    TouchscreenEventMsgType event;
    touchPosition touch;
};

class GenericEventMsg : public IMessage {
  public:
    GenericEventMsg(MessageType event_in) : event(event_in){};
    ~GenericEventMsg() = default;
    MessageType getMessageType() { return event; };
    MessageType event;
};
