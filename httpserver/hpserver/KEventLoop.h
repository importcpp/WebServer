#pragma once

#include "../utils/Knoncopyable.h"

namespace kb
{

class EventLoop : noncopyable
{

    EventLoop();
    ~EventLoop();

    void loop();
};

} // namespace kb
