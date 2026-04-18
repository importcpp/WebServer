#include "KTcpServer.h"
#include "webserver/loop/KEventLoop.h"
#include "webserver/loop/KEventLoopThreadPool.h"
#include "webserver/utils/KAsyncLogger.h"
#include "webserver/utils/KTypes.h"
#include "KAcceptor.h"
#include "KSocketsOps.h"

using namespace kback;

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr)
    : loop_(CheckNotNull<EventLoop>(loop)), ipPort_(listenAddr.toHostPort()),
      name_(listenAddr.toHostPort()),
      acceptor_(new Acceptor(loop, listenAddr)), started_(false),
      nextConnId_(1), threadPool_(new EventLoopThreadPool(loop)) {
  acceptor_->setNewConnectionCallback(
      [this](int sockfd, const InetAddress &peerAddr) {
        newConnection(sockfd, peerAddr);
      });
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                     const string &nameArg)
    : loop_(CheckNotNull<EventLoop>(loop)), ipPort_(listenAddr.toHostPort()),
      name_(nameArg), acceptor_(new Acceptor(loop, listenAddr)),
      started_(false), nextConnId_(1),
      threadPool_(new EventLoopThreadPool(loop)) {
  acceptor_->setNewConnectionCallback(
      [this](int sockfd, const InetAddress &peerAddr) {
        newConnection(sockfd, peerAddr);
      });
}

TcpServer::~TcpServer() {}

void TcpServer::setThreadNum(int numThreads) {
  assert(numThreads >= 0);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start() {
  if (!started_) {
    started_ = true;
    threadPool_->start();
  }
  if (!acceptor_->listenning()) {
    loop_->runInLoop([this] { acceptor_->listen(); });
  }
}

void TcpServer::configureConnection(const TcpConnectionPtr &conn) {
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback([this](const TcpConnectionPtr &connection) {
    removeConnection(connection);
  });
  conn->setRecycleCallback(
      [this](TcpConnectionPtr connection) { recycleConnection(std::move(connection)); });
}

void TcpServer::recycleConnection(TcpConnectionPtr conn) {
  connectionRecycler_.recycle(std::move(conn));
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
  loop_->assertInLoopThread();
  char buf[32];
  snprintf(buf, sizeof buf, "#%d", nextConnId_);
  ++nextConnId_;
  std::string connName = name_ + buf;
  KBACK_LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s",
                 name_.c_str(), connName.c_str(), peerAddr.toHostPort().c_str());
  // 当新的连接达到时，先注册一个TcpConnection对象，然后用io线程进行管理
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  EventLoop *ioLoop = threadPool_->getNextLoop();

  TcpConnectionPtr conn;
  if (connectionRecycler_.tryTake(conn)) {
    conn->setNewTcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr);
  } else {
    conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr,
                                          peerAddr);
  }
  connections_[connName] = conn;
  configureConnection(conn);
  ioLoop->runInLoop([conn] { conn->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn) {
  loop_->runInLoop([this, conn] { removeConnectionInLoop(conn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {
  loop_->assertInLoopThread();
  KBACK_LOG_INFO("TcpServer::removeConnection [%s] - connection %s",
                 name_.c_str(), conn->name().c_str());
  // 此时，conn 对象被其本身还有 connections_ 对象持有，
  // 当把conn从 connections_ 中移除时引用计数降到1,
  // 不做处理的话，离开作用域后就会被销毁
  // 最后使用了 std::bind 让TcpConnection的生命期延长到connectDestroyed
  // 调用完成时
  size_t n = connections_.erase(conn->name());
  assert(n == 1);
  (void)n;
  EventLoop *ioLoop = conn->getLoop();
  ioLoop->queueInLoop([conn] { conn->connectDestroyed(); });
}
