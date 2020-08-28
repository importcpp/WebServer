#pragma once

#include "../lock/KSpinLock.h"
#include "../utils/KCallbacks.h"
#include "../utils/Knoncopyable.h"
#include "KInetAddress.h"
#include "KTcpConnection.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace kback {
class Acceptor;
class EventLoop;
class EventLoopThreadPool;

// TcpServer class 的 主要用于负责Tcp连接的建立、维护以及销毁
// 它管理Acceptor类获得tcp连接，进而建立TcpConnection类管理tcp连接
class TcpServer : noncopyable {
public:
  TcpServer(EventLoop *loop, const InetAddress &listenAddr);
  TcpServer(EventLoop *loop, const InetAddress &listenAddr,
            const string &nameArg);
  ~TcpServer();

  const string &ipPort() const { return ipPort_; }
  const string &name() const { return name_; }

  // 启动server
  void start();

  EventLoop *getLoop() const { return loop_; }

  void setThreadNum(int numThreads);

  // 回调函数机制
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }

  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

#ifdef USE_RECYCLE
  // recycle 函数
  void recycleCallback(TcpConnectionPtr conn) {
    if (oddEven == false) {
      oddEven = true;
      return;
    }
    oddEven = false;
    spinlock.lock();
    backup_conn_.push_back(conn);
    spinlock.unlock();
  }
#endif

private:
  // 服务器对新连接的连接处理函数
  void newConnection(int sockfd, const InetAddress &peerAddr);

  // 移除tcp连接函数
  void removeConnection(const TcpConnectionPtr &conn);
  void removeConnectionInLoop(const TcpConnectionPtr &conn);

  typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

  EventLoop *loop_; // 主线程loop对象，用于管理accept
  const string ipPort_;
  const string name_;
  std::unique_ptr<Acceptor> acceptor_;
  ConnectionCallback connectionCallback_;

  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  bool started_;
  int nextConnId_;
  bool oddEven = false;
  // tcp连接字典
  ConnectionMap connections_;
#ifdef USE_RECYCLE
  SpinLock spinlock;
  std::vector<TcpConnectionPtr> backup_conn_;
#endif
  std::unique_ptr<EventLoopThreadPool> threadPool_;
};

} // namespace kback