#include "KBuffer.h"
#include "KSocketsOps.h"
// #include "logging/KLogging.h"
#include "../utils/KTypes.h"
#include <errno.h>
#include <memory.h>
#include <sys/uio.h>

using namespace kback;

const char Buffer::kCRLF[] = "\r\n"; // 回车换行

ssize_t Buffer::readFd(int fd, int *savedErrno)
{
    char extrabuf[65536]; // 64*1024
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;
    // readv()代表分散读， 即将数据从文件描述符读到分散的内存块中
    const ssize_t n = readv(fd, vec, 2);
    if (n < 0)
    {
        *savedErrno = errno;
    }
    else if (implicit_cast<size_t>(n) <= writable)
    {
        writerIndex_ += n;
    }
    else
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

// 这两块存储区分别是临时的栈存储区和buffer的可写区。
// 如果可写空间能够完全接收数据(即n<=writable)，就无需后续操作了。
// 反之，需要把放不下的数据(此时在extrabuf里)append到缓存，
// 此时一定会转移可读数据或者进行扩容。

// 之所以要这么设定，我觉得作者是想减少一次系统调用，避免该线程在系统调用上占用太多时间。

