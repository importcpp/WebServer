#include "KEventLoop.h"
#include "KAsyncWaker.h"
#include "webserver/poller/KChannel.h"
#include "webserver/poller/KEventManager.h"
#include "webserver/utils/KAsyncLogger.h"

using namespace kback;

// 通过 __thread 来保证一个线程只能创建一个 EventLoop
// __thread 变量是每个线程有的一份独立的实体
// 类似于static 只在编译期初始化
thread_local EventLoop *t_loopInThisThread = nullptr;
const int kPollTimeMs = 1000;

EventLoop::EventLoop()
    : looping_(false), threadId_(std::this_thread::get_id()),
      eventmanager_(new EventManager(this)), quit_(false),
      callingPendingFunctors_(false), asyncWaker_(new AsyncWaker(this)) {
  KBACK_LOG_TRACE("EventLoop created %p in thread %zu", this,
                  std::hash<std::thread::id>{}(threadId_));
  if (t_loopInThisThread) {
    KBACK_LOG_FATAL("Another EventLoop %p exists in thread %zu",
                    t_loopInThisThread,
                    std::hash<std::thread::id>{}(threadId_));
  } else {
    t_loopInThisThread = this;
  }
}

EventLoop::~EventLoop() {
  assert(!looping_);
  t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  quit_.store(false, std::memory_order_release);

  while (!quit_.load(std::memory_order_acquire)) {
    activeChannels_.clear();
    // 这里poll在轮询中，为阻塞的，想要回调活动事件，必须想办法唤醒
    // muduo这里采用了一个很巧妙的办法，专门用一个文件描述符来进行唤醒
    pollReturnTime_ = eventmanager_->poll(kPollTimeMs, &activeChannels_);
    for (auto it = activeChannels_.begin(); it != activeChannels_.end(); ++it) {
      (*it)->handleEvent(pollReturnTime_);
    }
    doPendingFunctors();
  }
  KBACK_LOG_TRACE("EventLoop %p stop looping", this);
  looping_ = false;
}

void EventLoop::quit() {
  quit_.store(true, std::memory_order_release);
  if (!isInLoopThread()) {
    asyncWaker_->wakeup();
  }
}

// channel 中注册了事件，需要更新，但是
// 为了保证线程安全，channel自己是不会更新的，
// 将更新的工作转交给loop, 然后在loop线程中 同步poll 中的channel
void EventLoop::updateChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  // EventLoop 实际上不负责更新channel
  eventmanager_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel) {
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  eventmanager_->removeChannel(channel);
}

void EventLoop::abortNotInLoopThread() {
  KBACK_LOG_FATAL(
      "EventLoop::abortNotInLoopThread - EventLoop %p was created in "
      "threadId_=%zu, current thread id=%zu",
      this, std::hash<std::thread::id>{}(threadId_),
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
  std::abort();
}

// 执行装载的的回调函数，下面的处理方法很巧妙
void EventLoop::doPendingFunctors() {
  callingPendingFunctors_ = true;
  pendingFunctors_.consumeAll([](Functor &functor) {
    if (functor != nullptr) {
      functor();
    }
  });
  callingPendingFunctors_ = false;
}

// !这个函数是在loop启动之前调用的，用于设置channel
void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread()) {
    cb();
  } else {
    queueInLoop(std::move(cb));
  }
}

// queueInLoop 是public的，不一定要被runInLoop调用，也可以被直接调用
void EventLoop::queueInLoop(Functor cb) {
  pendingFunctors_.push(std::move(cb));

  if (!isInLoopThread() || callingPendingFunctors_) {
    asyncWaker_->wakeup();
  }
}
