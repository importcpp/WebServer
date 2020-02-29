#pragma once

#include "KPoller.h"
#include "KPollPoller.h"
#include "KEPollPoller.h"

#include <stdlib.h>

using namespace kback;

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    if (::getenv("MUDUO_USE_POLL"))
    {
        return new EPollPoller(loop);
    }
    else
    {
        return new PollPoller(loop);
    }
}