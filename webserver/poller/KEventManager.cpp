#include "KEventManager.h"
#include "KChannel.h"
#include <assert.h>
#include <errno.h>
#include <iostream>
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
#ifdef USE_STD_COUT
    std::cout << "LOG_SYSFATAL:   "
              << "EventManager::EventManager" << std::endl;
#endif
  }
}

EventManager::~EventManager() { ::close(epollfd_); }

Timestamp EventManager::poll(int timeoutMs, ChannelList *activeChannels) {
#ifdef USE_STD_COUT
  std::cout << "LOG_TRACE:   "
            << "fd total count " << channels_.size() << std::endl;
#endif
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  int savedErrno = errno;
  Timestamp now(Timestamp::now());
  if (numEvents > 0) {
#ifdef USE_STD_COUT
    std::cout << "LOG_TRACE:   " << numEvents << " events happended"
              << std::endl;
#endif
    fillActiveChannels(numEvents, activeChannels);
    if (implicit_cast<size_t>(numEvents) == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
#ifdef USE_STD_COUT
    std::cout << "LOG_TRACE:   "
              << " nothing happended" << std::endl;
#endif
  } else {
    if (savedErrno != EINTR) {
      errno = savedErrno;
#ifdef USE_STD_COUT
      std::cout << "LOG_SYSERR:   "
                << "EventManager::poll()" << std::endl;
#endif
    }
  }
  return now;
}

void EventManager::fillActiveChannels(int numEvents,
                                      ChannelList *activeChannels) const {
  assert(implicit_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

void EventManager::updateChannel(Channel *channel) {
  assertInLoopThread();
  const int index = channel->index();
#ifdef USE_STD_COUT
  std::cout << "LOG_TRACE:   "
            << "fd = " << channel->fd() << " events = " << channel->events()
            << " index = " << index << std::endl;
#endif
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
#ifdef USE_STD_COUT
  std::cout << "LOG_TRACE:   "
            << "fd = " << fd << std::endl;
#endif
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
#ifdef USE_STD_COUT
  std::cout << "LOG_TRACE:   "
            << "epoll_ctl op = " << operationToString(operation)
            << " fd = " << fd << " event = { " << channel->eventsToString()
            << " }" << std::endl;
#endif
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
#ifdef USE_STD_COUT
      std::cout << "LOG_SYSERR"
                << "epoll_ctl op =" << operationToString(operation)
                << " fd =" << fd << std::endl;
#endif
    } else {
#ifdef USE_STD_COUT
      std::cout << "LOG_SYSFATAL"
                << "epoll_ctl op =" << operationToString(operation)
                << " fd =" << fd;
#endif
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