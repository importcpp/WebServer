#include "KTcpConnection.h"

#include "../utils/KTypes.h"
#include "../poller/KChannel.h"
#include "../loop/KEventLoop.h"
#include "KSocket.h"
#include "KSocketsOps.h"
#include <functional>
#include <errno.h>
#include <stdio.h>

using namespace kback;

ssize_t writeET(int fd, const char *begin, size_t len);

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckNotNull<EventLoop>(loop)),
      name_(nameArg),
      state_(kConnecting),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr)
{
#ifdef USE_STD_COUT

    std::cout << "LOG_DEBUG:   "
              << "TcpConnection::ctor[" << name_ << "] at " << this
              << " fd=" << sockfd << std::endl;
#endif
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, _1));
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));
}

TcpConnection::~TcpConnection()
{
#ifdef USE_STD_COUT
    std::cout << "LOG_DEBUG:   "
              << "TcpConnection::dtor[" << name_ << "] at " << this
              << " fd=" << channel_->fd() << std::endl;
#endif
}

void TcpConnection::send(const std::string &message)
{
    if (state_ == kConnected)
    {
        if (loop_->isInLoopThread())
        {
            sendInLoop(message);
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, message));
        }
    }
}

// sendInLoop() 会先尝试直接发送数据，如果一次发送完毕，就不会启用writeCallback
// 如果只发送了部分数据，则把剩余的数据放入outputBuffer_, 并开始关注writable事件，
// 以后在handlewrite中发送剩余的数据， 如果当前outputBuffer_已经有待发送的数据，
// 那么就不能先尝试发送了，因为会造成数据乱序
void TcpConnection::sendInLoop(const std::string &message)
{
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    // 如果输出队列为空，那么可以直接写进输出buffer
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {

#ifdef USE_EPOLL_LT
        nwrote = ::write(channel_->fd(), message.data(), message.size());
#else
        nwrote = writeET(channel_->fd(), message.data(), message.size());
#endif

        if (nwrote >= 0)
        {
            if (implicit_cast<size_t>(nwrote) < message.size())
            {
#ifdef USE_STD_COUT
                std::cout << "LOG_TRACE:   "
                          << "I am going to write more data" << std::endl;
#endif
            }
            else if (writeCompleteCallback_)
            {
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if (errno != EWOULDBLOCK)
            {
#ifdef USE_STD_COUT

                std::cout << "LOG_SYSERR:   "
                          << "TcpConnection::sendInLoop" << std::endl;
#endif
            }
        }
    }

    assert(nwrote >= 0);
    if (implicit_cast<size_t>(nwrote) < message.size())
    {
        outputBuffer_.append(message.data() + nwrote, message.size() - nwrote);
        // 如果没有关注writable事件，则开始关注
        if (!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// 没有直接关闭TCP connection, 数据写完成后，只关闭写
void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting())
    {
        // 优雅的关闭套接字
        socket_->shutdownWrite();
    }
}

// 禁用Nagle算法，避免连续发包出现的延迟，这对编写低延迟网络服务很重要
void TcpConnection::setTcpNoDelay(bool on)
{
    socket_->setTcpNoDelay(on);
}

// tcp connection 处理连接建立的过程
// 1. 利用state_变量 标志 连接的状态
// 2. 掉用channel_->enableReading() 将channel负责的文件描述符中的可读事件注册到loop中
//    并用IO复用机制poller类来监视文件描述符
// 3. 最后调用connectioncallback函数 （注意其中使用的shared_from_this()）
void TcpConnection::connectEstablished()
{
    loop_->assertInLoopThread();
    // setTcpNoDelay(true);
    assert(state_ == kConnecting);
    setState(kConnected);
#ifdef USE_EPOLL_LT
#else
    // 开启ET模式 -- (这两步顺序不能错)
    channel_->enableEpollET();
#endif
    channel_->enableReading();
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
    loop_->assertInLoopThread();
    assert(state_ == kConnected);
    setState(kDisconnected);
    channel_->disableAll();
    connectionCallback_(shared_from_this());

    // 移除poller对channel_指针的管理
    loop_->removeChannel(get_pointer(channel_));
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
#ifdef USE_EPOLL_LT
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
#else
    // ET模式读写，直到发生EAGAIN，才返回
    ssize_t n = inputBuffer_.readFdET(channel_->fd(), &savedErrno);
#endif
    if (n > 0)
    {
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if (n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
#ifdef USE_STD_COUT
        std::cout << "LOG_SYSERR:   "
                  << "TcpConnection::handleRead" << std::endl;
#endif
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting())
    {
        int savedErrno = 0;
#ifdef USE_EPOLL_LT
         ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
#else
        // ET模式读写，直到发生EAGAIN，才返回
        ssize_t n = outputBuffer_.writeFdET(channel_->fd(), &savedErrno);
#endif
        if (n > 0)
        {
            // retrieve过程(readIndex的设置)改为在write读取时就完成
            // outputBuffer_.retrieve(n);
            if (outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if (writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this()));
                }

                if (state_ = kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
            else
            {
#ifdef USE_STD_COUT
                std::cout << "LOG_TRACE:   "
                          << "I am going to write more data" << std::endl;
#endif
            }
        }
        else
        {
#ifdef USE_STD_COUT
            std::cout << "LOG_SYSERR:   "
                      << "TcpConnection::handleWrite" << std::endl;
#endif
        }
    }
    else
    {
#ifdef USE_STD_COUT
        std::cout << "LOG_TRACE:   "
                  << "Connection is down, no more writing" << std::endl;
#endif
    }
}

// handleclose 的工作
// 1. 取消channel中设置的关注事件
// 2. 利用回调(将移除工作放到loop中)完成TcpConnection的注销工作
//    2.1 移除TcpServer的map中TcpConnection中的管理
//    2.2 再掉用自身的
void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
#ifdef USE_STD_COUT
    std::cout << "LOG_TRACE:   "
              << "TcpConnection::handleClose state = " << state_ << std::endl;
#endif
    assert(state_ == kConnected || state_ == kDisconnecting);
    // 这里不采取关闭fd的方式，因为建立了RAII的socket对象用于管理fd的析构
    // 这里的关闭是处理完输入输出流，同样的这里不直接关闭也可以方便找出程序的漏洞
    channel_->disableAll();
    closeCallback_(shared_from_this());
}

void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
#ifdef USE_STD_COUT
    std::cout << "LOG_ERROR:   "
              << "TcpConnection::handleError [" << name_
              << "] - SO_ERROR = " << err << " " << std::endl;
#endif
}

#ifdef USE_EPOLL_LT
#else
// ET 模式下处理写事件
ssize_t writeET(int fd, const char *begin, size_t len)
{
    ssize_t writesum = 0;
    char *tbegin = (char *)begin;
    for (;;)
    {
        ssize_t n = ::write(fd, tbegin, len);
        if (n > 0)
        {
            writesum += n;
            tbegin += n;
            len -= n;
            if (len == 0)
            {
                return writesum;
            }
        }
        else if (n < 0)
        {
            if (errno == EAGAIN) //系统缓冲区满，非阻塞返回
            {
#ifdef USE_STD_COUT
                std::cout << "ET mode: errno == EAGAIN" << std::endl;
#endif
                break;
            }
            // 暂未考虑其他错误
            else
            {
                return -1;
            }
        }
        else
        {
            // 返回0的情况，查看write的man，可以发现，一般是不会返回0的
            return 0;
        }
    }
    return writesum;
}
#endif
