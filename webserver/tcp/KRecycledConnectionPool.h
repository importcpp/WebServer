#pragma once

#include <mutex>
#include <utility>
#include <vector>

#ifdef USE_SPINLOCK
#include "webserver/lock/KSpinLock.h"
#endif

namespace kback {

template <typename T, typename Lock> class LockedRecycledConnectionPool {
public:
  void push(T item) {
    std::lock_guard<Lock> lock(lock_);
    items_.push_back(std::move(item));
  }

  bool tryTake(T &item) {
    std::lock_guard<Lock> lock(lock_);
    if (items_.empty()) {
      return false;
    }

    item = std::move(items_.back());
    items_.pop_back();
    return true;
  }

private:
  Lock lock_;
  std::vector<T> items_;
};

#ifdef USE_SPINLOCK
template <typename T>
using RecycledConnectionPool = LockedRecycledConnectionPool<T, SpinLock>;
#else
template <typename T>
using RecycledConnectionPool = LockedRecycledConnectionPool<T, std::mutex>;
#endif

} // namespace kback
