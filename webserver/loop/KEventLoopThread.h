#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include "../utils/Knoncopyable.h"

namespace kback
{
class EventLoop;

class EventLoopThread : noncopyable
{
public:
    EventLoopThread();
    ~EventLoopThread();
    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;

};

} // namespace kback