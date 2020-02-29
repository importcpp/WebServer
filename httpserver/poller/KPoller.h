#pragma once

#include <map>
#include <vector>
#include "../utils/Knoncopyable.h"
#include "../utils/KTimestamp.h"
#include "../KEventLoop.h"

namespace kback
{
class Channel;

// IO复用的抽象基类
// Poller类同样不拥有Channel类

class Poller : noncopyable
{
public:
    typedef std::vector<Channel *> ChannelList;

    Poller(EventLoop *loop);
    virtual ~Poller();

    /// Polls the I/O events.
    /// Must be called in the loop thread.
    virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;

    // update 关注的IO事件
    virtual void updateChannel(Channel *channel) = 0;
    // 当channel析构时，移除channel
    virtual void removeChannel(Channel *channel) = 0;

    virtual bool hasChannel(Channel *channel) const;

    static Poller *newDefaultPoller(EventLoop *loop);

protected:
    typedef std::map<int, Channel *> ChannelMap;
    ChannelMap channels_; // 从fd到channel*的映射

private:
    EventLoop *ownerLoop_;
};

} // namespace kback
