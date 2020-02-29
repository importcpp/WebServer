#include "KTcpServer.h"

#include "../KEventLoop.h"
#include "KAcceptor.h"
#include "KSocketsOps.h"
#include "../utils/KTypes.h"

using namespace kback;

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr)
    : loop_(CheckNotNull<EventLoop>(loop)),
      name_(listenAddr.toHostPort()),
      acceptor_(new Acceptor(loop, listenAddr)),
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
      started_(false),
      nextConnId_(1)
{
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
}

void TcpServer::start()
{
    if (!started_)
    {
        started_ = true;
    }
    if (!acceptor_->listenning())
    {
        loop_->runInLoop(std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    char buf[32];
    snprintf(buf, sizeof buf, "#%d", nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    std::cout << "LOG_INFO:   "
              << "TcpServer::newConnection [" << name_
              << "] - new connection [" << connName
              << "] from " << peerAddr.toHostPort() << std::endl;

    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    TcpConnectionPtr conn(new TcpConnection(loop_, connName, sockfd, localAddr, peerAddr));
    
    // 新建Tcp Connection, 此时新的Tcp connection是shared_ptr， 离开作用域后仅被connections_持有
    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, _1));
    conn->connectEstablished();
}


void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    std::cout << "LOG_INFO:   "
              << "TcpServer::removeConnection [" << name_
              << "] - connection " << conn->name() << std::endl;
    // 此时，conn 对象被其本身还有 connections_ 对象持有，
    // 当把conn从 connections_ 中移除时引用计数降到1, 不做处理的话，离开作用域后就会被销毁
    // 最后使用了 std::bind 让TcpConnection的生命期延长到connectDestroyed 调用完成时
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    (void)n;

    loop_->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));
}