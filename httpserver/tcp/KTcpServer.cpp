#include "KTcpServer.h"
#include "../utils/KTypes.h"
#include "../loop/KEventLoop.h"
#include "../loop/KEventLoopThreadPool.h"
#include "KAcceptor.h"
#include "KSocketsOps.h"

using namespace kback;

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr)
    : loop_(CheckNotNull<EventLoop>(loop)),
      name_(listenAddr.toHostPort()),
      acceptor_(new Acceptor(loop, listenAddr)),
      threadPool_(new EventLoopThreadPool(loop)),
      started_(false),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr, const string &nameArg)
    : loop_(CheckNotNull<EventLoop>(loop)),
      ipPort_(listenAddr.toHostPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr)),
      threadPool_(new EventLoopThreadPool(loop)),
      started_(false),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
}

void TcpServer::setThreadNum(int numThreads)
{
    assert(numThreads >= 0);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
    if (!started_)
    {
        started_ = true;
        threadPool_->start();
    }
    if (!acceptor_->listenning())
    {
        loop_->runInLoop(std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    loop_->assertInLoopThread();
    char buf[32];
    snprintf(buf, sizeof buf, "#%d", nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    std::cout << "LOG_INFO:   "
              << "TcpServer::newConnection [" << name_
              << "] - new connection [" << connName
              << "] from " << peerAddr.toHostPort() << std::endl;

    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    EventLoop *ioLoop = threadPool_->getNextLoop();
    // TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, _1));
    // conn->connectEstablished();
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    //   LOG_INFO << "TcpServer::removeConnection [" << name_
    //            << "] - connection " << conn->name();
    std::cout << "LOG_INFO:   "
              << "TcpServer::removeConnection [" << name_
              << "] - connection " << conn->name() << std::endl;
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    (void)n;
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));

    // loop_->queueInLoop(
    //     std::bind(&TcpConnection::connectDestroyed, conn));
}