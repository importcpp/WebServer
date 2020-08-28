#include "KEventLoopThread.h"
#include "KEventLoop.h"

#include <functional>

using namespace kback;

EventLoopThread::EventLoopThread() : loop_(nullptr), exiting_(false) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  loop_->quit();
  thread_.join();
}

EventLoop *EventLoopThread::startLoop() {
  thread_ = std::thread(std::bind(&EventLoopThread::threadFunc, this));
  {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this] { return this->loop_ != nullptr; });
  }
  return loop_;
}

void EventLoopThread::threadFunc() {
  EventLoop loop;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = &loop;
    cond_.notify_one();
  }
  loop.loop();
}