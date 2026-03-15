#pragma once

#include <3ds.h>
#include <memory>

typedef RecursiveLock LockType;

class ThreadLock {
  public:
    ThreadLock();
    ~ThreadLock();

    void lock();
    void unlock();

  private:
    LockType _lock;
};
