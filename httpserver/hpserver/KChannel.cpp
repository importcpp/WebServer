
#include "KChannel.h"

#include <poll.h>


using namespace kb;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;




void Channel::handleEvent(Timestamp receiveTime)
{
    eventHandling_ = true;

    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) // POLLRDHUP ???
    {
        if (readCallback_)
        {
            readCallback_(receiveTime);
        }
    }
    eventHandling_ = false;
}