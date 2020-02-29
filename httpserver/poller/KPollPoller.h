#pragma once

#include "KPoller.h"
#include <vector>

// Poller中并没有include<poll.h>, 所以要使用pollfd, 需要前向声明
struct pollfd;

namespace kback
{
class PollPoller : public Poller
{
public:
    PollPoller(EventLoop *loop);
    ~PollPoller() override;

    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    void fillActiveChannels(int numEvents,
                            ChannelList *activeChannels) const;

    typedef std::vector<struct pollfd> PollFdList;
    PollFdList pollfds_;
};

} // namespace kback
