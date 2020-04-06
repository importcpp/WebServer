#include "../loop/KEventLoop.h"
#include "KChannel.h"

#include <poll.h>
#include <iostream>
#include <sstream>
#include "../utils/KTypes.h"

using namespace kback;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop *loop, int fdArg)
    : loop_(CheckNotNull<EventLoop>(loop)),
      fd_(fdArg),
      events_(0),
      revents_(0),
      index_(-1),
      eventHandling_(false)
{
    // std::cout << "Init channel:   " << fd_ << std::endl;
}

Channel::~Channel()
{
    // 确保channel被析构时不是在事件处理期间
    assert(!eventHandling_);
}

void Channel::update()
{
    loop_->updateChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    eventHandling_ = true;
    if (revents_ & POLLNVAL) // invalid polling request
    {
#ifdef USE_STD_COUT
        std::cout << "LOG_WARN:   "
                  << "Channel::handle_event() POLLNVAL" << std::endl;
#endif
    }
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
    {
#ifdef USE_STD_COUT
        std::cout << "LOG_WARN:   "
                  << "Channel::handle_event() POLLHUP" << std::endl;
#endif
        if (closeCallback_)
            closeCallback_();
    }
    if (revents_ & (POLLERR | POLLNVAL))
    {
        if (errorCallback_)
        {
            errorCallback_();
        }
    }

    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) 
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if (revents_ & POLLOUT)
    {
        if (writeCallback_)
        {
            writeCallback_();
        }
    }
    eventHandling_ = false;
}

string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN)
        oss << "IN ";
    if (ev & POLLPRI)
        oss << "PRI ";
    if (ev & POLLOUT)
        oss << "OUT ";
    if (ev & POLLHUP)
        oss << "HUP ";
    if (ev & POLLRDHUP)
        oss << "RDHUP ";
    if (ev & POLLERR)
        oss << "ERR ";
    if (ev & POLLNVAL)
        oss << "NVAL ";

    return oss.str();
}