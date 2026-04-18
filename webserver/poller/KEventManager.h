#pragma once

#include "webserver/loop/KEventLoop.h"
#include "webserver/utils/KTimestamp.h"
#include "webserver/utils/Knoncopyable.h"
#include <unordered_map>
#include <vector>

struct epoll_event;

namespace kback {
class Channel;
// IO复用的类
// EventManager类不拥有Channel类

class EventManager {
public:
  typedef std::vector<Channel *> ChannelList;

public:
  EventManager(EventLoop *loop);
  ~EventManager();

  Timestamp poll(int timeoutMs, ChannelList *activeChannels);
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);

  void assertInLoopThread() const { ownerLoop_->assertInLoopThread(); }

private:
  static const int kInitEventListSize = 16;

  static const char *operationToString(int op);

  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  void update(int operation, Channel *channel);

  typedef std::vector<struct epoll_event> EventList;

private:
  typedef std::unordered_map<int, Channel *> ChannelMap;
  ChannelMap channels_; // 从fd到channel*的映射
  EventLoop *ownerLoop_;

private:
  int epollfd_;
  EventList events_;
};

} // namespace kback
