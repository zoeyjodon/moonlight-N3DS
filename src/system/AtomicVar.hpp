#pragma once

#include "ThreadLock.hpp"
#include <3ds.h>
#include <memory>

template <typename T> class AtomicVar {
  public:
    AtomicVar(T value_in) : value(value_in) {}
    ~AtomicVar() = default;

    T load() {
        lock.lock();
        T retval = value;
        lock.unlock();
        return retval;
    }

    void store(T value_in) {
        lock.lock();
        value = value_in;
        lock.unlock();
    }

  private:
    ThreadLock lock;
    T value;
};
