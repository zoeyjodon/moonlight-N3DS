#include "dispatcher.hpp"
#include <algorithm>

std::shared_ptr<MessageDispatcher> MessageDispatcher::instance = nullptr;

MessageDispatcher::MessageDispatcher()
    : subscriber_lock(ThreadLock::CreateLock()),
      message_lock(ThreadLock::CreateLock()) {
    for (uint8_t i = 0; i < MessageType::MESSAGE_TYPE_COUNT; i++) {
        subscribers[static_cast<MessageType>(i)] = std::vector<ISubscriber *>();
    }
}

void MessageDispatcher::subscribe(MessageType type, ISubscriber *sub) {
    if (sub == nullptr) {
        return;
    }

    auto tmp_lock = ThreadLock(subscriber_lock.get());
    std::vector<ISubscriber *> &sub_list = subscribers[type];
    auto sub_pos = std::find(sub_list.begin(), sub_list.end(), sub);
    // Prevent duplication
    if (sub_pos == sub_list.end()) {
        subscribers[type].push_back(sub);
    }
}

void MessageDispatcher::unsubscribe(MessageType type, ISubscriber *sub) {
    if (sub == nullptr) {
        return;
    }
    auto tmp_lock = ThreadLock(subscriber_lock.get());
    std::vector<ISubscriber *> &sub_list = subscribers[type];
    auto sub_pos = std::find(sub_list.begin(), sub_list.end(), sub);
    if (sub_pos != sub_list.end()) {
        sub_list.erase(sub_pos);
    }
}

void MessageDispatcher::post_immediate(std::shared_ptr<IMessage> m) {
    auto tmp_lock = ThreadLock(subscriber_lock.get());
    std::vector<ISubscriber *> &sub_list = subscribers[m->getMessageType()];
    for (ISubscriber *sub : sub_list) {
        if (sub == nullptr) {
            continue;
        }
        sub->accept(m.get());
    }
}

void MessageDispatcher::post(std::shared_ptr<IMessage> m) {
    auto tmp_lock = ThreadLock(message_lock.get());
    message_queue.push(m);
}

bool MessageDispatcher::_is_queue_empty() {
    auto tmp_lock = ThreadLock(message_lock.get());
    return message_queue.empty();
}

void MessageDispatcher::dispatch_all() {
    while (!_is_queue_empty()) {
        std::shared_ptr<IMessage> m;
        {
            auto tmp_lock = ThreadLock(message_lock.get());
            m = message_queue.front();
            message_queue.pop();
        }
        post_immediate(m);
    }
}
