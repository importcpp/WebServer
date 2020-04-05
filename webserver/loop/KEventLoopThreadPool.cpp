#include "KEventLoop.h"
#include "KEventLoopThread.h"
#include "KEventLoopThreadPool.h"

#include <functional>

using namespace kback;

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop)
    : baseLoop_(baseLoop),
      started_(false),
      numThreads_(0),
      next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
}

void EventLoopThreadPool::start()
{
    assert(!started_);
    baseLoop_->assertInLoopThread();

    started_ = true;

    for (int i = 0; i < numThreads_; ++i)
    {
        EventLoopThread *t = new EventLoopThread;
        threads_.emplace_back(t);
        loops_.push_back(t->startLoop());
    }
}

EventLoop *EventLoopThreadPool::getNextLoop()
{
    baseLoop_->assertInLoopThread();
    EventLoop *loop = baseLoop_;

    if (!loops_.empty())
    {
        // round-robin 分配 loop
        loop = loops_[next_];
        next_ = (next_ + 1) % (loops_.size());
    }
    return loop;
}