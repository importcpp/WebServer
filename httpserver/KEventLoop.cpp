#include "KEventLoop.h"
#include "poller/KChannel.h"
#include "poller/KDefaultPoller.h"
#include <sys/eventfd.h>
#include <signal.h>

using namespace kback;

const int kPollTimeMs = 3000;

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      poller_(Poller::newDefaultPoller(this))
{
    std::cout << "LOG_TRACE:   "
              << "EventLoop created " << this << std::endl;
}

EventLoop::~EventLoop()
{
    assert(!looping_);
}

void EventLoop::loop()
{
    assert(!looping_);
    looping_ = true;
    quit_ = false;

    while (!quit_)
    {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (auto it = activeChannels_.begin(); it != activeChannels_.end(); ++it)
        {
            (*it)->handleEvent(pollReturnTime_);
        }
        doPendingFunctors();
    }
    std::cout << "LOG_TRACE:   "
              << "EventLoop " << this << " stop looping" << std::endl;
    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;
}

// channel 中注册了事件，需要更新
// 将更新的工作转交给loop, 然后在loop中 同步poll 中的channel
void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    // EventLoop 实际上不负责更新channel
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    poller_->removeChannel(channel);
}

// 执行装载的的回调函数
void EventLoop::doPendingFunctors()
{
    // 遍历执行回调函数
    for (auto &functor : pendingFunctors_)
    {
        functor();
    }
    pendingFunctors_.clear();
}

// !这个函数是在loop启动之前调用的，用于设置channel
void EventLoop::runInLoop(const Functor &cb)
{
    cb();
}

// queueInLoop 是public的，不一定要被runInLoop调用，也可以被直接调用
void EventLoop::queueInLoop(const Functor &cb)
{
    pendingFunctors_.push_back(cb);
}
