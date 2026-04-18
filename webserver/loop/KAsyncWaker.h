#pragma once

#include <memory>
#include <sys/eventfd.h>

namespace kback {
class EventLoop;
class Channel;
class AsyncWaker {
private:
  int wakerfd_;
  EventLoop *loop_;
  std::unique_ptr<Channel> wakerchannel_;
  void handleRead();

public:
  AsyncWaker(EventLoop *loop);
  ~AsyncWaker();

  void wakeup();
};

} // namespace kback
