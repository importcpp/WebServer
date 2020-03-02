#pragma once

#include "../utils/Knoncopyable.h"
#include <thread>
#include <iostream>
#include <assert.h>

#include <vector>
#include <memory>
#include <mutex>
#include <thread>

#include "../utils/KTimestamp.h"

#include <functional>

namespace kback
{


class Channel;
class Poller;

class EventLoop : noncopyable
{
public:
    typedef std::function<void()> Functor;

    EventLoop();
    ~EventLoop();

    void loop();

    void quit();

    // 如果用户在当前IO线程调用这个函数，回调会同步进行;
    // 如果用户在其他线程调用runInLoop(), cb会被加入队列，IO线程会被唤醒来调用这个Functor
    /// Safe to call from other threads.
    void runInLoop(const Functor &cb);
    /// Queues callback in the loop thread. Runs after finish pooling.
    /// Safe to call from other threads.
    // note: 将cb放入队列，并在必要时唤醒IO线程
    void queueInLoop(const Functor &cb);


    // internal use only
    void wakeup();
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);

    void assertInLoopThread()
    {
        if (!isInLoopThread())
        {
            abortNotInLoopThread();
        }
    }

    // 判断是否是在loop thread中
    bool isInLoopThread() const
    {
        return threadId_ == std::this_thread::get_id();
    }

private:
    void abortNotInLoopThread();
    bool looping_;
    const std::thread::id threadId_;

    typedef std::vector<Channel*> ChannelList;
    ChannelList activeChannels_;

    // ??? 为什么要用unique_ptr
    std::unique_ptr<Poller> poller_;

    bool quit_; // atomic
    Timestamp pollReturnTime_;

    // 全部用于唤醒机制 
    void handleRead(); // 用于wakeup channel的回调
    void doPendingFunctors();
    bool callingPendingFunctors_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    std::mutex mutex_;
    std::vector<Functor> pendingFunctors_;

};
} // namespace kback
