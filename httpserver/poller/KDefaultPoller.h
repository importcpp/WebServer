#pragma once

#include "KPoller.h"
#include "KPollPoller.h"
#include "KEPollPoller.h"

#include <stdlib.h>

using namespace kback;

Poller *Poller::newDefaultPoller(EventLoop *loop)
{
    return new EPollPoller(loop);
}