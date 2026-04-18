#pragma once

#include <mutex>
#include <utility>
#include <vector>

#include "webserver/lock/KLockFreeQueue.h"
#ifdef USE_SPINLOCK
#include "webserver/lock/KSpinLock.h"
#endif

namespace kback {

template <typename T, typename Lock> class LockedPendingFunctorQueue {
public:
  template <typename U> void push(U &&item) {
    std::lock_guard<Lock> lock(lock_);
    queue_.emplace_back(std::forward<U>(item));
  }

  template <typename Consumer> void consumeAll(Consumer &&consumer) {
    std::vector<T> localQueue;
    {
      std::lock_guard<Lock> lock(lock_);
      localQueue.swap(queue_);
    }

    for (T &item : localQueue) {
      consumer(item);
    }
  }

private:
  Lock lock_;
  std::vector<T> queue_;
};

template <typename T> class LockFreePendingFunctorQueue {
public:
  template <typename U> void push(U &&item) {
    queue_.Enqueue(std::forward<U>(item));
  }

  template <typename Consumer> void consumeAll(Consumer &&consumer) {
    for (;;) {
      T item;
      if (!queue_.Try_Dequeue(item)) {
        break;
      }
      consumer(item);
    }
  }

private:
  LockFreeQueue<T> queue_;
};

#ifdef USE_LOCKFREEQUEUE
template <typename T> using PendingFunctorQueue = LockFreePendingFunctorQueue<T>;
#else
#ifdef USE_SPINLOCK
template <typename T>
using PendingFunctorQueue = LockedPendingFunctorQueue<T, SpinLock>;
#else
template <typename T>
using PendingFunctorQueue = LockedPendingFunctorQueue<T, std::mutex>;
#endif
#endif

} // namespace kback
