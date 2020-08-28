#pragma once

#include <assert.h>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <thread>
#include <vector>

#include "../utils/KTimestamp.h"
#include "../utils/Knoncopyable.h"

#ifdef USE_LOCKFREEQUEUE
#include "../lock/KLockFreeQueue.h"
#endif

#ifdef USE_SPINLOCK
#include "../lock/KSpinLock.h"
#endif

namespace kback {

class Channel;
class EventManager;
class AsyncWaker;
class EventLoop : noncopyable {
public:
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();

  void loop();

  void quit();

  // 如果用户在当前IO线程调用这个函数，回调会同步进行;
  // 如果用户在其他线程调用runInLoop(),
  // cb会被加入队列，IO线程会被唤醒来调用这个Functor
  void runInLoop(const Functor &cb);
  // note: 将cb放入队列，并在必要时唤醒IO线程
  void queueInLoop(const Functor &cb);

  void wakeup();
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);

  void assertInLoopThread() {
    if (!isInLoopThread()) {
      abortNotInLoopThread();
    }
  }

  // 判断是否是在loop thread中
  bool isInLoopThread() const {
    return threadId_ == std::this_thread::get_id();
  }

private:
  void abortNotInLoopThread();
  bool looping_;
  const std::thread::id threadId_;

  typedef std::vector<Channel *> ChannelList;
  ChannelList activeChannels_;

  // 用unique_ptr自动管理指向EventManager的指针
  // 不需要担心EventManager_的销毁问题
  std::unique_ptr<EventManager> eventmanager_;

  bool quit_; // atomic
  Timestamp pollReturnTime_;

  // 全部用于唤醒机制
  void handleRead(); // 用于wakeup channel的回调
  void doPendingFunctors();
  bool callingPendingFunctors_;

  std::unique_ptr<AsyncWaker> asyncwaker;

#ifdef USE_LOCKFREEQUEUE
  LockFreeQueue<Functor> pendingFunctors_;
#else
// 锁类型的选择
#ifdef USE_SPINLOCK
  SpinLock spinlock;
#else
  std::mutex mutex_;
#endif
  std::vector<Functor> pendingFunctors_;
#endif
};
} // namespace kback
