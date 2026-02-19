#pragma once

#include "message.hpp"

class ISubscriber {
  public:
    virtual void accept(IMessage *msg) = 0;
};
