#pragma once

#include "ThreadLock.hpp"

ThreadLock::ThreadLock(LockType *pLock_in) : pLock(pLock_in) {
    RecursiveLock_Lock(pLock);
};

ThreadLock::~ThreadLock() { RecursiveLock_Unlock(pLock); };

PLockType ThreadLock::CreateLock() {
    auto lock = std::make_shared<LockType>();
    RecursiveLock_Init(lock.get());
    return lock;
};
