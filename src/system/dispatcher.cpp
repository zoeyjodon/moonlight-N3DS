#include "dispatcher.hpp"
#include <algorithm>

std::shared_ptr<MessageDispatcher> MessageDispatcher::instance = nullptr;

MessageDispatcher::MessageDispatcher() {
    for (uint8_t i = 0; i < MessageType::MESSAGE_TYPE_COUNT; i++) {
        subscribers[static_cast<MessageType>(i)] = std::vector<ISubscriber *>();
    }
}

void MessageDispatcher::subscribe(MessageType type, ISubscriber *sub) {
    if (sub == nullptr) {
        return;
    }

    subscriber_lock.lock();
    std::vector<ISubscriber *> &sub_list = subscribers[type];
    auto sub_pos = std::find(sub_list.begin(), sub_list.end(), sub);
    // Prevent duplication
    if (sub_pos == sub_list.end()) {
        subscribers[type].push_back(sub);
    }
    subscriber_lock.unlock();
}

void MessageDispatcher::unsubscribe(MessageType type, ISubscriber *sub) {
    if (sub == nullptr) {
        return;
    }

    subscriber_lock.lock();
    std::vector<ISubscriber *> &sub_list = subscribers[type];
    auto sub_pos = std::find(sub_list.begin(), sub_list.end(), sub);
    if (sub_pos != sub_list.end()) {
        sub_list.erase(sub_pos);
    }
    subscriber_lock.unlock();
}

void MessageDispatcher::post_immediate(std::shared_ptr<IMessage> m) {
    subscriber_lock.lock();
    std::vector<ISubscriber *> &sub_list = subscribers[m->getMessageType()];
    for (ISubscriber *sub : sub_list) {
        if (sub == nullptr) {
            continue;
        }
        sub->accept(m.get());
    }
    subscriber_lock.unlock();
}

void MessageDispatcher::post(std::shared_ptr<IMessage> m) {
    message_lock.lock();
    message_queue.push(m);
    message_lock.unlock();
}

bool MessageDispatcher::_is_queue_empty() {
    message_lock.lock();
    bool is_empty = message_queue.empty();
    message_lock.unlock();
    return is_empty;
}

void MessageDispatcher::dispatch_all() {
    while (!_is_queue_empty()) {

        message_lock.lock();
        std::shared_ptr<IMessage> m = message_queue.front();
        message_queue.pop();
        message_lock.unlock();

        post_immediate(m);
    }
}
