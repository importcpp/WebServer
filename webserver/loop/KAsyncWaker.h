#pragma once

#include <sys/eventfd.h>
#include <iostream>

namespace kback
{
class EventLoop;
class Channel;
class AsyncWaker
{
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