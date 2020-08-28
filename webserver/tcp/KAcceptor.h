#pragma once

#include "../utils/Knoncopyable.h"
#include <functional>

#include "../poller/KChannel.h"
#include "KSocket.h"

namespace kback {
class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
public:
  typedef std::function<void(int sockfd, const InetAddress &)>
      NewConnectionCallback;
  Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
  Acceptor(EventLoop *loop, const InetAddress &listenAddr);

  void setNewConnectionCallback(const NewConnectionCallback &cb) {
    newConnectionCallback_ = cb;
  }

  bool listenning() const { return listenning_; }
  void listen();

private:
  // 具备loop和channel
  void handleRead();

  EventLoop *loop_;

  // 成员函数的初始化顺序与他们在类定义中的出现顺序一致
  // acceptSocket_必须在acceptChannel_之前定义，
  // 因为构造函数列表初始化的时候 acceptChannel_要使用acceptSocket_的成员初始化
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;
  bool listenning_;
};

} // namespace kback
