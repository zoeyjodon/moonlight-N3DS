#pragma once

#include <3ds.h>
#include <memory>

typedef RecursiveLock LockType;
typedef std::shared_ptr<LockType> PLockType;

class ThreadLock {
  public:
    ThreadLock(LockType *pLock_in);
    ~ThreadLock();

    static PLockType CreateLock();

  private:
    LockType *pLock;
};
