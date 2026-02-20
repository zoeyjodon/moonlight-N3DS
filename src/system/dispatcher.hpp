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

    static std::shared_ptr<MessageDispatcher> get_instance() {
        if (instance == nullptr) {
            instance = std::make_shared<MessageDispatcher>();
        }
        return instance;
    }

    void subscribe(MessageType type, ISubscriber *sub);
    void unsubscribe(MessageType type, ISubscriber *sub);
    void post_immediate(IMessage *m);

  private:
    static std::shared_ptr<MessageDispatcher> instance;
    std::map<MessageType, std::vector<ISubscriber *>> subscribers{};
};
