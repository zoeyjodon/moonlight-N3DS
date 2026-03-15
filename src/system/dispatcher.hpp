#pragma once

#include "ThreadLock.hpp"
#include "message.hpp"
#include "subscriber.hpp"
#include <map>
#include <memory>
#include <queue>
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
    void post_immediate(std::shared_ptr<IMessage> m);
    void post(std::shared_ptr<IMessage> m);
    void dispatch_all();

  private:
    bool _is_queue_empty();

  private:
    static std::shared_ptr<MessageDispatcher> instance;
    std::map<MessageType, std::vector<ISubscriber *>> subscribers{};
    std::queue<std::shared_ptr<IMessage>> message_queue{};
    ThreadLock subscriber_lock;
    ThreadLock message_lock;
};
