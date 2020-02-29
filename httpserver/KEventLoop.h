#pragma once

#include "utils/Knoncopyable.h"
#include "utils/KTimestamp.h"
#include <iostream>
#include <assert.h>
#include <vector>
#include <memory>
#include <functional>

namespace kback
{

class Channel;
class Poller;

class EventLoop : noncopyable
{
public:
    typedef std::function<void()> Functor;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    void runInLoop(const Functor &cb);
    void queueInLoop(const Functor &cb);

    // internal use only
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);

private:
    bool looping_;

    typedef std::vector<Channel*> ChannelList;
    ChannelList activeChannels_;

    std::unique_ptr<Poller> poller_;

    bool quit_; // atomic
    Timestamp pollReturnTime_;

    void doPendingFunctors();
    std::vector<Functor> pendingFunctors_;

};
} // namespace kback
