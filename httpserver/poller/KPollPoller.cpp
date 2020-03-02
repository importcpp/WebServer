#include "KPollPoller.h"
#include <iostream>
#include "KChannel.h"
#include <assert.h>
#include <errno.h>
#include <poll.h>

using namespace kback;

PollPoller::PollPoller(EventLoop *loop)
    : Poller(loop)
{
}

PollPoller::~PollPoller() = default;

Timestamp PollPoller::poll(int timeoutMs, ChannelList *activeChannels)
{
    // XXX pollfds_ shouldn't change
    // ::poll是为了区分Poller中的poll, ::表示全局作用域
    // pollfds - vector<struct pollfd>
    // C++标准保证std::vector的元素排列跟数组一样
    int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());
    if (numEvents > 0)
    {
        // LOG_TRACE << numEvents << " events happended";
        std::cout << "LOG_TRACE:   " << numEvents << " events happended" << std::endl;
        fillActiveChannels(numEvents, activeChannels);
    }
    else if (numEvents == 0)
    {
        // LOG_TRACE << " nothing happended";
        std::cout << "LOG_TRACE:   " << numEvents << " nothing happended" << std::endl;
    }
    else
    {
        // report error and abort
        if (saveErrno != EINTR)
        {
            std::cout << "LOG_SYSERR:   "
                      << "PollPoller::poll()" << std::endl;
        }
    }
    return now;
}

// 函数功能： 遍历pollfds_, 找出有活动事件的fd, 把它对应的Channel填入对应的activeChannels,
// 这个函数的复杂度是O(N), 其中N是pollfds_的长度，即文件描述符的长度
void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList *activeChannels) const
{
    for (PollFdList::const_iterator pfd = pollfds_.begin();
         pfd != pollfds_.end() && numEvents > 0; ++pfd)
    {
        if (pfd->revents > 0)
        {
            --numEvents;
            ChannelMap::const_iterator ch = channels_.find(pfd->fd);
            assert(ch != channels_.end());
            Channel *channel = ch->second;
            assert(channel->fd() == pfd->fd);
            channel->set_revents(pfd->revents);
            // pfd->revents = 0;
            activeChannels->push_back(channel);
        }
    }
}

// 感觉这个函数名不应该叫做updateChannel
// 个人理解，用户通过channel监视事件，然后管理事件的回调
// 显然channel是没有监视事件的功能的，因此，监视这件事要交给poller来做， poller主要监视pollfds_中的文件描述符
// 这里是将channel 与 pollfds_ 同步，起到让poller"监视"channel的作用

void PollPoller::updateChannel(Channel *channel)
{
    Poller::assertInLoopThread();
    // LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
    std::cout << "LOG_TRACE:   "
              << "fd = " << channel->fd() << " events = " << channel->events() << std::endl;
    if (channel->index() < 0)
    {
        // a new one, add to pollfds_
        assert(channels_.find(channel->fd()) == channels_.end());
        struct pollfd pfd;
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        pollfds_.push_back(pfd);
        int idx = static_cast<int>(pollfds_.size()) - 1;
        channel->set_index(idx);
        channels_[pfd.fd] = channel;
    }
    else
    {
        // update existing one
        assert(channels_.find(channel->fd()) != channels_.end());
        assert(channels_[channel->fd()] == channel);
        int idx = channel->index();
        assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
        struct pollfd &pfd = pollfds_[idx];
        assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd() - 1);
        pfd.fd = channel->fd();
        pfd.events = static_cast<short>(channel->events());
        pfd.revents = 0;
        if (channel->isNoneEvent())
        {
            // ignore this pollfd
            pfd.fd = -channel->fd() - 1;
        }
    }
}

void PollPoller::removeChannel(Channel *channel)
{
    Poller::assertInLoopThread();
    // LOG_TRACE << "fd = " << channel->fd();
    std::cout << "LOG_TRACE:   "
              << "fd = " << channel->fd() << std::endl;
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    assert(channel->isNoneEvent());
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    const struct pollfd &pfd = pollfds_[idx];
    (void)pfd;
    assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->events());
    size_t n = channels_.erase(channel->fd());
    assert(n == 1);
    (void)n;
    if (implicit_cast<size_t>(idx) == pollfds_.size() - 1)
    {
        pollfds_.pop_back();
    }
    else
    {
        int channelAtEnd = pollfds_.back().fd;
        iter_swap(pollfds_.begin() + idx, pollfds_.end() - 1);
        if (channelAtEnd < 0)
        {
            channelAtEnd = -channelAtEnd - 1;
        }
        channels_[channelAtEnd]->set_index(idx);
        pollfds_.pop_back();
    }
}