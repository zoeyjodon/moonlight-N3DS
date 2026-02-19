#pragma once

#include "message.hpp"
#include "subscriber.hpp"
#include <map>
#include <memory>
#include <vector>

class MessageDispatcher {
  public:
    MessageDispatcher();
    ~MessageDispatcher() = default;

    static MessageDispatcher *get_instance() {
        if (instance == nullptr) {
            instance = std::make_unique<MessageDispatcher>();
        }
        return instance.get();
    }

    void subscribe(MessageType type, ISubscriber *sub);
    void unsubscribe(MessageType type, ISubscriber *sub);
    void post_immediate(IMessage *m);

  private:
    static std::unique_ptr<MessageDispatcher> instance;
    std::map<MessageType, std::vector<ISubscriber *>> subscribers{};
};
