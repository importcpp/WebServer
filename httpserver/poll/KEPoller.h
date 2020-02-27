#pragma once

#include <map>
#include <vector>

#include "KTimestamp.h"
#include "KEventLoop.h"
#include "Knoncopyable.h"

struct epoll_event;

namespace kback
{
class Channel;

///
/// IO Multiplexing with epoll(4).
///
/// This class doesn't own the Channel objects.
class EPoller : noncopyable
{
public:
    typedef std::vector<Channel *> ChannelList;

    EPoller(EventLoop *loop);
    ~EPoller();
    /// Polls the I/O events.
    /// Must be called in the loop thread.
    Timestamp poll(int timeoutMs, ChannelList *activeChannels);

    /// Changes the interested I/O events.
    /// Must be called in the loop thread.
    void updateChannel(Channel *channel);
    /// Remove the channel, when it destructs.
    /// Must be called in the loop thread.
    void removeChannel(Channel *channel);

    void assertInLoopThread() { ownerLoop_->assertInLoopThread(); }

private:
    static const int kInitEventListSize = 16;

    void fillActiveChannels(int numEvents,
                            ChannelList *activeChannels) const;
    void update(int operation, Channel *channel);

    typedef std::vector<struct epoll_event> EventList;
    typedef std::map<int, Channel *> ChannelMap;

    EventLoop *ownerLoop_;
    int epollfd_;
    EventList events_;
    ChannelMap channels_;
};

} // namespace kback