#pragma once

#include "ThreadLock.hpp"

ThreadLock::ThreadLock() { RecursiveLock_Init(&_lock); };

ThreadLock::~ThreadLock(){};

void ThreadLock::lock() { RecursiveLock_Lock(&_lock); }
void ThreadLock::unlock() { RecursiveLock_Unlock(&_lock); }
