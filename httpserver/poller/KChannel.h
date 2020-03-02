#pragma once
#include <functional>
#include "../utils/Knoncopyable.h"
#include "../utils/KTimestamp.h"

namespace kback
{
class EventLoop;

// A selectable I/O channel.
// 每个Channel对象都只属于某一个IO线程，每个channel
// 对象自始至终只负责一个文件描述符fd的IO事件分发，
// 但它不会拥有这个fd，也不会在析构的时候关闭这个fd
class Channel : noncopyable
{
public:
    typedef std::function<void()> EventCallback;
    typedef std::function<void(Timestamp)> ReadEventCallback;

    Channel(EventLoop *loop, int fdArg);
    ~Channel();

    // handleEvent是Channel的核心，它由EventLoop::loop()调用
    // 它的功能是根据revents_的值分别调用不同的用户回调
    void handleEvent(Timestamp receiveTime);
    void setReadCallback(const ReadEventCallback &cb)
    {
        readCallback_ = cb;
    }
    void setWriteCallback(const EventCallback &cb)
    {
        writeCallback_ = cb;
    }
    void setErrorCallback(const EventCallback &cb)
    {
        errorCallback_ = cb;
    }
    void setCloseCallback(const EventCallback &cb)
    {
        closeCallback_ = cb;
    }

    int fd() const { return fd_; }
    int events() const { return events_; };
    void set_revents(int revt) { revents_ = revt; }
    bool isNoneEvent() const { return events_ == kNoneEvent; }

    // 注册可读事件   注意update()函数
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    void enableWriting()
    {
        events_ |= kWriteEvent;
    }
    void disableWriting()
    {
        events_ &= (-kWriteEvent);
        update();
    }
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }

    bool isWriting() const { return events_ & kWriteEvent; }
    // for poller
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // for debug
    string eventsToString() const;

    EventLoop *ownerLoop() { return loop_; }

private:
    static string eventsToString(int fd, int ev);
    void update();

    // 每个channel对象 共享相同的常量值，所以定义为static
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_;
    const int fd_;
    int events_;  // Channel类关心的IO事件
    int revents_; // 目前活动的事件
    int index_;   // used by Poller

    bool eventHandling_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback errorCallback_;
    EventCallback closeCallback_;
};

} // namespace kback
