#pragma once
#include "../utils/Kcopyable.h"

#include <algorithm>
#include <string>
#include <vector>

#include <assert.h>
//#include <unistd.h>  // ssize_t

namespace kback
{

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
class Buffer : public kback::copyable
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    Buffer()
        : buffer_(kCheapPrepend + kInitialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {
        assert(readableBytes() == 0);
        assert(writableBytes() == kInitialSize);
        assert(prependableBytes() == kCheapPrepend);
    }

    // default copy-ctor, dtor and assignment are fine

    void swap(Buffer &rhs)
    {
        buffer_.swap(rhs.buffer_);
        std::swap(readerIndex_, rhs.readerIndex_);
        std::swap(writerIndex_, rhs.writerIndex_);
    }

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    const char *peek() const
    {
        return begin() + readerIndex_;
    }

    const char *findCRLF() const
    {
        // FIXME: replace with memmem()?
        const char *crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF + 2);
        return crlf == beginWrite() ? NULL : crlf;
    }

    // retrieve returns void, to prevent
    // string str(retrieve(readableBytes()), readableBytes());
    // the evaluation of two functions are unspecified
    void retrieve(size_t len)
    {
        assert(len <= readableBytes());
        readerIndex_ += len;
    }

    void retrieveUntil(const char *end)
    {
        assert(peek() <= end);
        assert(end <= beginWrite());
        retrieve(end - peek());
    }

    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAsString()
    {
        std::string str(peek(), readableBytes());
        retrieveAll();
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
        std::copy(data, data + len, beginWrite());
        hasWritten(len);
    }

    void append(const void * /*restrict*/ data, size_t len)
    {
        append(static_cast<const char *>(data), len);
    }

    // 当剩余可写空间小于写入数据时，系统要对缓存进行扩容，
    // 执行了makeSpace这个方法，参数len是要写入数据的长度
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

    void hasWritten(size_t len)
    {
        writerIndex_ += len;
    }

    void prepend(const void * /*restrict*/ data, size_t len)
    {
        assert(len <= prependableBytes());
        readerIndex_ -= len;
        const char *d = static_cast<const char *>(data);
        std::copy(d, d + len, begin() + readerIndex_);
    }

    void shrink(size_t reserve)
    {
        std::vector<char> buf(kCheapPrepend + readableBytes() + reserve);
        std::copy(peek(), peek() + readableBytes(), buf.begin() + kCheapPrepend);
        buf.swap(buffer_);
    }

    /// Read data directly into buffer.
    ///
    /// It may implement with readv(2)
    /// @return result of read(2), @c errno is saved
    ssize_t readFd(int fd, int *savedErrno);

private:
    char *begin()
    {
        return &*buffer_.begin();
    }

    const char *begin() const
    {
        return &*buffer_.begin();
    }

    // makeSpace 分为两种情况
    // 1. 如果可写长度和预备空间长度小于数据长度加上kCheapPrepend
    // (也就是说预备长度最低为kCheapPrepend)，就使用vector::resize分配刚好足够使用新的空间。
    // 2. 如果预备空间和可写长度够用呢，就无需分配新空间了，重利用旧空间即可，
    // 把可读空间前移到kCheapPrepend处，然后修改readIndex和writerIndex。
    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            // move readable data to the front, make space inside buffer
            assert(kCheapPrepend < readerIndex_);
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
            assert(readable == readableBytes());
        }
    }

private:
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;

    static const char kCRLF[];
};

} // namespace kback