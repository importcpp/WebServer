#ifdef USE_RINGBUFFER
#pragma once
#include "../utils/Kcopyable.h"

#include <algorithm>
#include <string>
#include <vector>
#include <string.h>
#include <assert.h>
//#include <unistd.h>  // ssize_t

namespace kback
{
class Buffer : public kback::copyable
{
public:
    static const size_t kForPrepend = 1;
    static const size_t kInitialSize = 1024;

    Buffer() : capacity_(kForPrepend + kInitialSize),
               readerIndex_(kForPrepend),
               writerIndex_(kForPrepend),
               buffer_(new char[capacity_])
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == kInitialSize);
    }

    ~Buffer()
    {
        delete buffer_;
        buffer_ = nullptr;
        capacity_ = 0;
    }

    size_t readableBytes() const
    {
        if (writerIndex_ >= readerIndex_)
        {
            return writerIndex_ - readerIndex_;
        }

        return capacity_ - readerIndex_ + writerIndex_;
    }

    size_t writableBytes() const
    {
        if (writerIndex_ >= readerIndex_)
        {
            return capacity_ - writerIndex_ + readerIndex_ - kForPrepend;
        }
        return readerIndex_ - writerIndex_ - kForPrepend;
    }

    const char *peek() const
    {
        return buffer_ + readerIndex_;
    }

    const char *findCRLF() const
    {
        if (writerIndex_ > readerIndex_)
        {
            const char *crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
            return crlf == beginWrite() ? nullptr : crlf;
        }
        else
        {
            if ((buffer_[capacity_ - 1] == kCRLF[0]) && (buffer_[0] == kCRLF[1]))
            {
                return buffer_ + capacity_ - 1;
            }
            const char *crlft = std::search(peek(), end(), kCRLF, kCRLF + 2);
            if (crlft != end())
            {
                return crlft;
            }
            const char *crlf = std::search(begin(), beginWrite(), kCRLF, kCRLF + 2);
            return crlf == beginWrite() ? NULL : crlf;
        }
    }

    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        readerIndex_ += len;
        readerIndex_ %= capacity_;
    }

    void retrieveUntil(const char *end)
    {
        // !!! 没有做合法性检查
        if ((end >= begin()) && end < peek())
        {
            retrieve(end - peek() + capacity_);
        }
        retrieve(end - peek());
    }

    void retrieveAll()
    {
        readerIndex_ = kForPrepend;
        writerIndex_ = kForPrepend;
    }

    std::string retrieveAsString()
    {
        if (writerIndex_ < readerIndex_)
        {
            std::string str(peek(), capacity_ - readerIndex_);
            std::string strbegin(buffer_, writerIndex_);
            str += strbegin;
            retrieveAll();
            return str;
        }

        std::string str(peek(), writerIndex_ - readerIndex_);
        retrieveAll();
        return str;
    }

    std::string bufferToString()
    {
        if (writerIndex_ < readerIndex_)
        {
            std::string str(peek(), capacity_ - readerIndex_);
            std::string strbegin(buffer_, writerIndex_);
            str += strbegin;
            return str;
        }

        std::string str(peek(), writerIndex_ - readerIndex_);
        return str;
    }

    void append(const std::string &str)
    {
        append(str.data(), str.length());
    }

    // append 主要做了三点
    // 确保缓存拥有足够的空间存储新数据
    // 拷贝数据到可写位置
    // 更新writeIndex位置
    void append(const char * /*restrict*/ data, size_t len)
    {
        ensureWritableBytes(len);
        // 可以和std::copy对比下，看看哪个性能更高
        if (writerIndex_ < readerIndex_)
        {
            memcpy(beginWrite(), data, len);
        }
        else
        {
            // 看看尾部预留的空间大小
            size_t reserve_tail = capacity_ - writerIndex_;
            if (reserve_tail >= len)
            {
                memcpy(beginWrite(), data, len);
                writerIndex_ += len;
            }
            else
            {
                memcpy(beginWrite(), data, reserve_tail);
                // writerIndex_ 先一步更新完毕
                writerIndex_ = len - reserve_tail;
                memcpy(buffer_, data + reserve_tail, writerIndex_);
            }
        }
    }

    void append(const void * /*restrict*/ data, size_t len)
    {
        append(static_cast<const char *>(data), len);
    }

    // 1. 当剩余可写空间小于写入数据时，系统要对缓存进行扩容，
    //     执行了makeSpace这个方法，参数len是要写入数据的长度
    // 2. 如果可写长度够用呢，就无需分配新空间了
    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    char *beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char *beginWrite() const
    {
        return begin() + writerIndex_;
    }

    /// 将数据读入buffer, 适合LT
    ssize_t readFd(int fd, int *savedErrno);
    /// 将数据读入buffer, 适合ET
    ssize_t readFdET(int fd, int *savedErrno);

    /// 将数据读出buffer, 适合LT
    ssize_t writeFd(int fd, int *savedErrno);
    /// 将数据读出buffer, 适合ET
    ssize_t writeFdET(int fd, int *savedErrno);

private:
    char *begin()
    {
        return buffer_;
    }

    const char *begin() const
    {
        return buffer_;
    }

    char *end()
    {
        return buffer_ + capacity_;
    }
    const char *end() const
    {
        return buffer_ + capacity_;
    }

    // makeSpace
    // 重新分配空间，然后修改readIndex和writerIndex
    void makeSpace(size_t len)
    {
        // 增长空间
        size_t n = (capacity_ << 1) + len;
        char *newbuffer_ = new char[n];
        if (writerIndex_ >= readerIndex_)
        {
            memcpy(newbuffer_ + kForPrepend, buffer_ + readerIndex_, (writerIndex_ - readerIndex_));
        }
        else
        {
            // writerIndex_在前的时候需要考虑 可读数据分为后 前两段的情况
            memcpy(newbuffer_ + kForPrepend, buffer_ + readerIndex_, capacity_ - readerIndex_);
            memcpy(newbuffer_ + kForPrepend + capacity_ - readerIndex_, buffer_, writerIndex_);
        }
        size_t readable_bytes = readableBytes();
        readerIndex_ = kForPrepend;
        writerIndex_ = readerIndex_ + readable_bytes;
        capacity_ = n;
        delete[] buffer_;
        buffer_ = newbuffer_;
    }

private:
    size_t readerIndex_;
    size_t writerIndex_;
    size_t capacity_;
    char *buffer_;
    static const char kCRLF[];
};

} // namespace kback
#endif