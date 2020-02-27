#pragma once

#include "../utils/Knoncopyable.h"
#include <vector>
#include "../utils/KTimestamp.h"

struct epoll_event;

namespace kb
{

class EPoller : noncopyable
{

public:
    EPoller();
    ~EPoller();

    Timestamp poll(int timeoutMs, );

    // 用于储存活动事件
    typedef std::vector<struct epoll_event> EventList;



};

} // namespace kb
