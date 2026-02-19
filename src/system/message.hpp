#pragma once

#include <3ds.h>

enum MessageType { TOUCHSCREEN_EVENT, MESSAGE_TYPE_COUNT };

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
