#pragma once

#include "../utils/KTimestamp.h"
#include <functional>
#include <memory>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

template <typename T> inline T *get_pointer(const std::shared_ptr<T> &ptr) {
  return ptr.get();
}

template <typename T> inline T *get_pointer(const std::unique_ptr<T> &ptr) {
  return ptr.get();
}

namespace kback {

// All client visible callbacks go here.
class Buffer;
class TcpConnection;

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

typedef std::function<void()> TimerCallback;
typedef std::function<void(const TcpConnectionPtr &)> ConnectionCallback;

typedef std::function<void(const TcpConnectionPtr &, Buffer *buf, Timestamp)>
    MessageCallback;

typedef std::function<void(const TcpConnectionPtr &)> WriteCompleteCallback;

typedef std::function<void(const TcpConnectionPtr &)> CloseCallback;

typedef std::function<void(const TcpConnectionPtr)> RecycleCallback;

} // namespace kback