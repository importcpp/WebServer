#pragma once

#include <iostream>
#include <sys/eventfd.h>

namespace kback {
class EventLoop;
class Channel;
class AsyncWaker {
private:
  /* data */
  int wakerfd_;
  EventLoop *loop_;
  Channel *wakerchannel_;
  void handleRead();

public:
  AsyncWaker(EventLoop *loop);
  ~AsyncWaker();

  void wakeup();
};

} // namespace kback