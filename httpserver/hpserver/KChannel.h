#pragma once
#include "Knoncopyable.h"

#include <functional>
#include "KTimestamp.h"

namespace kb
{

// channel的作用主要是 面向对象，可以将事件全部统一 eventfd的处理和Tcp connection的处理统一起来
class Channel : noncopyable
{
    typedef std::function<void(Timestamp)> ReadEventCallback;

public:
    Channel(int fdArg);
    ~Channel();

    void Channel::handleEvent(Timestamp receiveTime);

    void setReadCallback(const ReadEventCallback &cb)
    {
        readCallback_ = cb;
    }

    // 注册可读事件，这一步必须要转交到Poll类
    void enableReading()
    {
        events_ |= kReadEvent;
        // 如何转交
        // update();
    }

    int fd() const { return fd_; }
    void set_revents(int revt) { revents_ = revt; }

private:

    // 每个channel对象 共享相同的常量值，所以定义为static
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    const int fd_;
    int events_;  // Channel类关心的IO事件
    int revents_; // 目前活动的事件
    int index_;   // used by Poller 主要用于标记

    bool eventHandling_;

    ReadEventCallback readCallback_;
};

} // namespace kb