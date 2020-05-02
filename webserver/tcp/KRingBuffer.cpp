#ifdef USE_RINGBUFFER
#include "KRingBuffer.h"
#include "KSocketsOps.h"
// #include "logging/KLogging.h"
#include "../utils/KTypes.h"
#include <errno.h>
#include <memory.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace kback;

const char Buffer::kCRLF[] = "\r\n"; // 回车换行

#ifdef USE_EPOLL_LT
// 通过readv 减少一次系统调用，避免该线程在系统调用上占用太多时间。
// 这两块存储区分别是临时的栈存储区和buffer的可写区。
// 如果可写空间能够完全接收数据(即n<=writable)，就无需后续操作了。
// 反之，需要把放不下的数据(此时在extrabuf里)append到缓存，
// 此时一定会转移可读数据或者进行扩容。
ssize_t Buffer::readFd(int fd, int *savedErrno)
{
    char extrabuf[65536]; // 64*1024
    struct iovec vec[3];
    const size_t writable = writableBytes();

    vec[0].iov_base = begin() + writerIndex_;

    if (writerIndex_ < readerIndex_)
    {
        vec[0].iov_len = writable;
        // 如果可写数据只在头端，则不使用第二块缓冲区
        vec[1].iov_base = begin();
        vec[1].iov_len = 0;
    }
    else // 如果可写数据横跨缓冲区尾端和头端
    {
        vec[0].iov_len = capacity_ - writerIndex_;
        vec[1].iov_base = begin();
        vec[1].iov_len = writable - vec[0].iov_len;
    }
    vec[2].iov_base = extrabuf;
    vec[2].iov_len = sizeof extrabuf;
    // readv()代表分散读， 即将数据从文件描述符读到分散的内存块中
    const ssize_t n = readv(fd, vec, 3);
    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (implicit_cast<size_t>(n) <= writable)
    {
        writerIndex_ += n;
        writerIndex_ %= capacity_;
    }
    else
    {
        writerIndex_ += writable;
        writerIndex_ %= capacity_;
        append(extrabuf, n - writable);
    }
    return n;
}

#else
// 支持ET模式下缓冲区的数据读取
ssize_t Buffer::readFdET(int fd, int *savedErrno)
{
    char extrabuf[65536];
    struct iovec vec[3];
    vec[2].iov_base = extrabuf;
    vec[2].iov_len = sizeof extrabuf;
    size_t writable;
    ssize_t readLen = 0;
    // 不断调用read读取数据
    for (;;)
    {
        // 写完后需要更新 vec[0] 便于下一次读入
        writable = writableBytes();
        if (writerIndex_ < readerIndex_)
        {
            vec[0].iov_base = begin() + writerIndex_;
            vec[0].iov_len = writable;
            // 不使用第二块缓冲区
            vec[1].iov_base = begin();
            vec[1].iov_len = 0;
        }
        else
        {
            vec[0].iov_base = begin() + writerIndex_;
            vec[0].iov_len = capacity_ - writerIndex_;
            vec[1].iov_base = begin();
            vec[1].iov_len = writable - vec[0].iov_len;
        }

        ssize_t n = readv(fd, vec, 3);
        if (n < 0)
        {
            if (errno == EAGAIN)
            {
                *savedErrno = errno;
                break;
            }
            return -1;
        }
        else if(n == 0)
        {
            // 没有读取到数据，认为对端已经关闭
            return 0;
        }
        else if (implicit_cast<size_t>(n) <= writable)
        {
            // 还没有写满缓冲区
            writerIndex_ += n;
            writerIndex_ %= capacity_;
        }
        else
        {
            // 已经写满缓冲区, 则需要把剩余的buf写进去
            writerIndex_ += writable;
            writerIndex_ %= capacity_;
            append(extrabuf, n - writable);
        }

        readLen += n;
    }
    return readLen;
}
#endif

ssize_t Buffer::writeFd(int fd, int *savedErrno)
{
    // 从可读位置开始读取
    struct iovec vec[2];
    if (writerIndex_ < readerIndex_)
    {
        vec[0].iov_base = buffer_ + readerIndex_;
        vec[0].iov_len = capacity_ - readerIndex_;
        vec[1].iov_base = buffer_;
        vec[1].iov_len = writerIndex_;
    }
    else
    {
        vec[0].iov_base = buffer_ + readerIndex_;
        // vec[0].iov_len = readableBytes();
        vec[0].iov_len = (writerIndex_ - readerIndex_);
        vec[1].iov_base = buffer_;
        vec[1].iov_len = 0;
    }
    ssize_t n = ::writev(fd, vec, 2);
    if (n > 0)
    {
        retrieve(n);
    }
    return 0;
}

#ifdef USE_EPOLL_LT
#else
// ET 模式下处理写事件
ssize_t Buffer::writeFdET(int fd, int *savedErrno)
{
    ssize_t writesum = 0;
    // 从可读位置开始读取
    struct iovec vec[2];
    for (;;)
    {
        if (writerIndex_ < readerIndex_)
        {
            vec[0].iov_base = buffer_ + readerIndex_;
            vec[0].iov_len = capacity_ - readerIndex_;
            vec[1].iov_base = buffer_;
            vec[1].iov_len = writerIndex_;
        }
        else
        {
            vec[0].iov_base = buffer_ + readerIndex_;
            // vec[0].iov_len = readableBytes();
            vec[0].iov_len = (writerIndex_ - readerIndex_);
            vec[1].iov_base = buffer_;
            vec[1].iov_len = 0;
        }
        ssize_t n = ::writev(fd, vec, 2);
        if (n > 0)
        {
            writesum += n;
            retrieve(n); // 更新可读索引
            if (readableBytes() == 0)
            {
                return writesum;
            }
        }
        else if (n < 0)
        {
            if (errno == EAGAIN) //系统缓冲区满，非阻塞返回
            {
#ifdef USE_STD_COUT
                std::cout << "ET mode: errno == EAGAIN" << std::endl;
#endif
                break;
            }
            // 暂未考虑其他错误
            else
            {
                return -1;
            }
        }
        else
        {
            // 返回0的情况，查看write的man，可以发现，一般是不会返回0的
            return 0;
        }
    }
    return writesum;
}
#endif
#endif