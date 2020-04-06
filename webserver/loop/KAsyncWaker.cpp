#include "KAsyncWaker.h"
#include "KEventLoop.h"
#include "../poller/KChannel.h"

using namespace kback;

AsyncWaker::AsyncWaker(EventLoop *loop)
    : loop_(loop),
      wakerfd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
      wakerchannel_(new Channel(loop, wakerfd_))
{
    if (wakerfd_ < 0)
    {
#ifdef USE_STD_COUT
        std::cout << "LOG_SYSERR:   "
                  << "Failed in eventfd" << std::endl;
#endif
        abort();
    }

    wakerchannel_->setReadCallback(std::bind(&AsyncWaker::handleRead, this));
    wakerchannel_->enableReading();
}

AsyncWaker::~AsyncWaker()
{
    ::close(wakerfd_);
}

void AsyncWaker::handleRead()
{
    loop_->assertInLoopThread();
    // 和wakeup函数对应，wakeup函数实际上是写事件，handleRead为对应的读事件
    uint64_t one = 1;
    ssize_t n = ::read(wakerfd_, &one, sizeof one);
    // 判断读取的字节是不是 one对应的字节数
    if (n != sizeof one)
    {
#ifdef USE_STD_COUT
        std::cout << "LOG_ERROR:   "
                  << "AsyncWaker::handleRead() reads " << n << " bytes instead of 8";
#endif
    }
}

void AsyncWaker::wakeup()
{
    uint64_t one = 1;
    ssize_t n = ::write(wakerfd_, &one, sizeof one);
    // 判断写入的字节是不是 one对应的字节数
    if (n != sizeof one)
    {
#ifdef USE_STD_COUT
        std::cout << "LOG_ERROR:   "
                  << "AsyncWaker::wakeup() writes " << n << " bytes instead of 8" << std::endl;
#endif
    }
}