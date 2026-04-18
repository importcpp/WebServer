#include "KEventManager.h"
#include "KChannel.h"
#include "webserver/utils/KAsyncLogger.h"
#include <assert.h>
#include <cstdlib>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

using namespace kback;

namespace {
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
} // namespace

EventManager::EventManager(EventLoop *loop)
    : ownerLoop_(loop), epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    KBACK_LOG_SYSFATAL("EventManager::EventManager");
    std::abort();
  }
}

EventManager::~EventManager() { ::close(epollfd_); }

Timestamp EventManager::poll(int timeoutMs, ChannelList *activeChannels) {
  KBACK_LOG_TRACE("fd total count %zu", channels_.size());
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  int savedErrno = errno;
  Timestamp now(Timestamp::now());
  if (numEvents > 0) {
    KBACK_LOG_TRACE("%d events happended", numEvents);
    fillActiveChannels(numEvents, activeChannels);
    if (implicit_cast<size_t>(numEvents) == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    KBACK_LOG_TRACE("nothing happended");
  } else {
    if (savedErrno != EINTR) {
      errno = savedErrno;
      KBACK_LOG_SYSERR("EventManager::poll()");
    }
  }
  return now;
}

void EventManager::fillActiveChannels(int numEvents,
                                      ChannelList *activeChannels) const {
  assert(implicit_cast<size_t>(numEvents) <= events_.size());
  activeChannels->reserve(activeChannels->size() +
                          static_cast<size_t>(numEvents));
  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

void EventManager::updateChannel(Channel *channel) {
  assertInLoopThread();
  const int index = channel->index();
  KBACK_LOG_TRACE("fd=%d events=%d index=%d", channel->fd(), channel->events(),
                  index);
  if (index == kNew || index == kDeleted) {
    // 使用EPOLL_CTL_ADD添加新的fd
    int fd = channel->fd();
    if (index == kNew) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    } else // index == kDeleted
    {
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }

    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    //  EPOLL_CTL_MOD/DEL更新当前关注的事件
    int fd = channel->fd();
    (void)fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == kAdded);
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

void EventManager::removeChannel(Channel *channel) {
  assertInLoopThread();
  int fd = channel->fd();
  KBACK_LOG_TRACE("fd=%d", fd);
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent());
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);
  size_t n = channels_.erase(fd);
  (void)n;
  assert(n == 1);

  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}

void EventManager::update(int operation, Channel *channel) {
  struct epoll_event event;
  memZero(&event, sizeof event);
  event.events = channel->events();
  event.data.ptr = channel;
  int fd = channel->fd();
  KBACK_LOG_TRACE("epoll_ctl op=%s fd=%d event={ %s }",
                  operationToString(operation), fd,
                  channel->eventsToString().c_str());
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      KBACK_LOG_SYSERR("epoll_ctl op=%s fd=%d", operationToString(operation),
                       fd);
    } else {
      KBACK_LOG_SYSFATAL("epoll_ctl op=%s fd=%d",
                         operationToString(operation), fd);
    }
  }
}

const char *EventManager::operationToString(int op) {
  switch (op) {
  case EPOLL_CTL_ADD:
    return "ADD";
  case EPOLL_CTL_DEL:
    return "DEL";
  case EPOLL_CTL_MOD:
    return "MOD";
  default:
    assert(false && "ERROR op");
    return "Unknown Operation";
  }
}
