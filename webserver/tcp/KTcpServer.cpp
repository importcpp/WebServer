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
#ifdef USE_STD_COUT
    std::cout << "LOG_INFO:   "
              << "TcpServer::newConnection [" << name_
              << "] - new connection [" << connName
              << "] from " << peerAddr.toHostPort() << std::endl;
#endif
    // 当新的连接达到时，先注册一个TcpConnection对象，然后用io线程进行管理
    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    EventLoop *ioLoop = threadPool_->getNextLoop();

#ifdef USE_RECYCLE
    if (backup_conn_.empty() == false)
    {
        TcpConnectionPtr conn(backup_conn_[0]);
        backup_conn_.erase(backup_conn_.begin());
        conn->setNewTcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr);
        connections_[connName] = conn;
        conn->setConnectionCallback(connectionCallback_);
        conn->setMessageCallback(messageCallback_);
        conn->setWriteCompleteCallback(writeCompleteCallback_);
        conn->setCloseCallback(
            std::bind(&TcpServer::removeConnection, this, _1));
        conn->setRecycleCallback(std::bind(&TcpServer::recycleCallback, this, _1));
        ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
        return;
    }
#endif
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, _1));
#ifdef USE_RECYCLE
    conn->setRecycleCallback(std::bind(&TcpServer::recycleCallback, this, _1));
#endif
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
#ifdef USE_STD_COUT

    std::cout << "LOG_INFO:   "
              << "TcpServer::removeConnection [" << name_
              << "] - connection " << conn->name() << std::endl;
#endif
    // 此时，conn 对象被其本身还有 connections_ 对象持有，
    // 当把conn从 connections_ 中移除时引用计数降到1, 不做处理的话，离开作用域后就会被销毁
    // 最后使用了 std::bind 让TcpConnection的生命期延长到connectDestroyed 调用完成时
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    (void)n;
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
