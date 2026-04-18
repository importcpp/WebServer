#include "KAsyncWaker.h"
#include "KEventLoop.h"
#include "webserver/poller/KChannel.h"
#include "webserver/utils/KAsyncLogger.h"

#include <errno.h>
#include <unistd.h>

using namespace kback;

AsyncWaker::AsyncWaker(EventLoop *loop)
    : wakerfd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)), loop_(loop),
      wakerchannel_(std::make_unique<Channel>(loop, wakerfd_)) {
  if (wakerfd_ < 0) {
    KBACK_LOG_SYSERR("Failed in eventfd");
    abort();
  }

  wakerchannel_->setReadCallback([this](Timestamp) { handleRead(); });
  wakerchannel_->enableReading();
}

AsyncWaker::~AsyncWaker() {
  wakerchannel_->disableAll();
  loop_->removeChannel(wakerchannel_.get());
  ::close(wakerfd_);
}

void AsyncWaker::handleRead() {
  loop_->assertInLoopThread();
  // 和wakeup函数对应，wakeup函数实际上是写事件，handleRead为对应的读事件
  uint64_t one = 1;
  ssize_t n = ::read(wakerfd_, &one, sizeof one);
  // 判断读取的字节是不是 one对应的字节数
  if (n != sizeof one) {
    KBACK_LOG_ERROR("AsyncWaker::handleRead() reads %zd bytes instead of 8",
                    n);
  }
}

void AsyncWaker::wakeup() {
  uint64_t one = 1;
  ssize_t n = ::write(wakerfd_, &one, sizeof one);
  // 判断写入的字节是不是 one对应的字节数
  if (n != sizeof one && errno != EAGAIN) {
    KBACK_LOG_ERROR("AsyncWaker::wakeup() writes %zd bytes instead of 8", n);
  }
}
