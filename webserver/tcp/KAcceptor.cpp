#include "KAcceptor.h"

#include "../loop/KEventLoop.h"
#include "KInetAddress.h"
#include "KSocketsOps.h"

using namespace kback;

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false)
{
    acceptSocket_.setReuseAddr(true);

    // tcp的bind过程，里面类型转换比较有意思
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(sockets::createNonblockingOrDie()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false)
{
    if (reuseport == true)
    {
        acceptSocket_.setReuseAddr(true);
    }

    // tcp的bind过程，里面类型转换比较有意思
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

// 在TCP编程中
// 在这里 要把接受端口请求的事件注册到accepchannel 可读时间的回调函数中
void Acceptor::listen()
{
    loop_->assertInLoopThread();
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();
}

void Acceptor::handleRead()
{
    loop_->assertInLoopThread();

    for (;;)
    {
        // 利用一个 InetAddress 来管理 sockaddr_in
        InetAddress peerAddr(0);
        // 这里是需要使用非阻塞IO的原因之一，即使没有连接到套接字，也应立即返回
        int connfd = acceptSocket_.accept(&peerAddr);
        if (connfd >= 0)
        {
            if (newConnectionCallback_)
            {
                newConnectionCallback_(connfd, peerAddr);
            }
            else
            {
                sockets::close(connfd);
            }
        }
        else
        {
            break;
        }
#ifdef USE_EPOLL_LT
        break;
#endif
    }
}