#pragma once

#include "message.hpp"

class ISubscriber {
  public:
    virtual ~ISubscriber() = default;
    virtual void accept(IMessage *msg) = 0;
};
