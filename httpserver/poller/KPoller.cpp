#include "KPoller.h"
#include "KChannel.h"

using namespace kback;

Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop)
{
}

Poller::~Poller() = default;

bool Poller::hasChannel(Channel *channel) const
{
    assertInLoopThread();
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}