#include "KEventLoop.h"
#include "../poller/KChannel.h"
#include "../poller/KDefaultPoller.h"
#include <sys/eventfd.h>
#include <signal.h>

using namespace kback;

// class IgnoreSigPipe
// {
// public:
//     IgnoreSigPipe()
//     {
//         ::signal(SIGPIPE, SIG_IGN);
//     }
// };

// IgnoreSigPipe initObj;

// 通过 __thread 来保证一个线程只能创建一个 EventLoop
// __thread 变量是每个线程有的一份独立的实体
// 类似于static 只在编译期初始化
__thread EventLoop *t_loopInThisThread = 0;
const int kPollTimeMs = 10000;

// ??? 查一下eventfd
static int createEventfd()
{
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0)
    {
        // LOG_SYSERR << "Failed in eventfd";
        std::cout << "LOG_SYSERR:   "
                  << "Failed in eventfd" << std::endl;
        abort();
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFunctors_(false),
      threadId_(std::this_thread::get_id()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_))
{
    std::cout << "LOG_TRACE:   "
              << "EventLoop created " << this << " in thread " << threadId_ << std::endl;

    if (t_loopInThisThread)
    {
        std::cout << "LOG_FATAL:   "
                  << "Another EventLoop " << t_loopInThisThread << " exists in this thread " << threadId_ << std::endl;
    }
    else
    {
        t_loopInThisThread = this;
    }

    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    // 保持wakeupfd 是可读的
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    assert(!looping_);
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::loop()
{
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while (!quit_)
    {
        activeChannels_.clear();
        // 这里poll在轮询中，为阻塞的，想要回调活动事件，必须想办法唤醒
        // muduo这里采用了一个很巧妙的办法，专门用一个文件描述符来进行唤醒
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (auto it = activeChannels_.begin(); it != activeChannels_.end(); ++it)
        {
            (*it)->handleEvent(pollReturnTime_);
        }
        doPendingFunctors();
    }
    // LOG_TRACE << "EventLoop " << this << " stop looping";
    std::cout << "LOG_TRACE:   "
              << "EventLoop " << this << " stop looping" << std::endl;
    looping_ = false;
}

void EventLoop::quit()
{
    quit_ = true;
    if (!isInLoopThread())
    {
        wakeup();
    }
}

// channel 中注册了事件，需要更新，但是 为了保证线程安全，channel自己是不会更新的，
// 将更新的工作转交给loop, 然后在loop线程中 同步poll 中的channel
void EventLoop::updateChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    // EventLoop 实际上不负责更新channel
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->removeChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
    std::cerr << "LOG_FATAL:   "
              << "EventLoop::abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ = " << threadId_
              << ", current thread id = " << std::this_thread::get_id() << std::endl;
    // std::abort();
}

void EventLoop::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    // 判断写入的字节是不是 one对应的字节数
    if (n != sizeof one)
    {
        // LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
        std::cout << "LOG_ERROR:   "
                  << "EventLoop::wakeup() writes " << n << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::handleRead()
{
    // 和wakeup函数对应，wakeup函数实际上是写事件，handleRead为对应的读事件
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    // 判断读取的字节是不是 one对应的字节数
    if (n != sizeof one)
    {
        // LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
        std::cout << "LOG_ERROR:   "
                  << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
    }
}

// 执行装载的的回调函数，下面的处理方法很巧妙
void EventLoop::doPendingFunctors()
{
    // std::vector<Functor> pendingFunctors_;
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    // 遍历执行回调函数
    for (auto &functor : functors)
    {
        functor();
    }

    callingPendingFunctors_ = false;
}

// !这个函数是在loop启动之前调用的，用于设置channel
void EventLoop::runInLoop(const Functor &cb)
{
    if (isInLoopThread())
    {
        cb();
    }
    else
    {
        queueInLoop(cb);
    }
}

// queueInLoop 是public的，不一定要被runInLoop调用，也可以被直接调用
void EventLoop::queueInLoop(const Functor &cb)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(cb);
    }

    if (!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();
    }
}
