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
    // LOG_DEBUG << "TcpConnection::ctor[" << name_ << "] at " << this
    //           << " fd=" << sockfd;
    std::cout << "LOG_DEBUG:   "
              << "TcpConnection::ctor[" << name_ << "] at " << this
              << " fd=" << sockfd << std::endl;
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
    // LOG_DEBUG << "TcpConnection::dtor[" << name_ << "] at " << this
    //           << " fd=" << channel_->fd();
    std::cout << "LOG_DEBUG:   "
              << "TcpConnection::dtor[" << name_ << "] at " << this
              << " fd=" << channel_->fd() << std::endl;
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

void TcpConnection::sendInLoop(const std::string &message)
{
    // sendInLoop() 会先尝试直接发送数据，如果一次发送完毕，就不会启用writeCallback
    // 如果只发送了部分数据，则把剩余的数据放入outputBuffer_, 并开始关注writable事件，
    // 以后在handlewrite中发送剩余的数据， 如果当前outputBuffer_已经有待发送的数据，
    // 那么就不能先尝试发送了，因为会造成数据乱序
    loop_->assertInLoopThread();
    ssize_t nwrote = 0;
    // if no thing in output queue, try writing directly
    if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), message.data(), message.size());
        if (nwrote >= 0)
        {
            if (implicit_cast<size_t>(nwrote) < message.size())
            {
                std::cout << "LOG_TRACE:   "
                          << "I am going to write more data" << std::endl;
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
                std::cout << "LOG_SYSERR:   "
                          << "TcpConnection::sendInLoop" << std::endl;
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
    // FIXME: use compare and swap
    if (state_ == kConnected)
    {
        setState(kDisconnecting);
        // FIXME: shared_from_this()?
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

// 没有直接关闭TCP connection, 数据写完成后，只关闭写
void TcpConnection::shutdownInLoop()
{
    loop_->assertInLoopThread();
    if (!channel_->isWriting())
    {
        // we are not writing
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
    assert(state_ == kConnecting);
    setState(kConnected);
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

    loop_->removeChannel(get_pointer(channel_));
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
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
        // LOG_SYSERR << "TcpConnection::handleRead";
        std::cout << "LOG_SYSERR:   "
                  << "TcpConnection::handleRead" << std::endl;
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    loop_->assertInLoopThread();
    if (channel_->isWriting())
    {
        ssize_t n = ::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());

        if (n > 0)
        {
            outputBuffer_.retrieve(n);
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
                // LOG_TRACE << "I am going to write more data";
                std::cout << "LOG_TRACE:   "
                          << "I am going to write more data" << std::endl;
            }
        }
        else
        {
            // LOG_SYSERR << "TcpConnection::handleWrite";
            std::cout << "LOG_SYSERR:   "
                      << "TcpConnection::handleWrite" << std::endl;
        }
    }
    else
    {
        // LOG_TRACE << "Connection is down, no more writing";
        std::cout << "LOG_TRACE:   "
                  << "Connection is down, no more writing" << std::endl;
    }
}

void TcpConnection::handleClose()
{
    loop_->assertInLoopThread();
    std::cout << "LOG_TRACE:   "
              << "TcpConnection::handleClose state = " << state_ << std::endl;

    assert(state_ == kConnected || state_ == kDisconnecting);
    // we don't close fd, leave it to dtor, so we can find leaks easily.
    channel_->disableAll();
    // must be the last line
    closeCallback_(shared_from_this());
}

void TcpConnection::handleError()
{
    int err = sockets::getSocketError(channel_->fd());
    // LOG_ERROR << "TcpConnection::handleError [" << name_
    //           << "] - SO_ERROR = " << err << " " << strerror_tl(err);
    std::cout << "LOG_ERROR:   "
              << "TcpConnection::handleError [" << name_
              << "] - SO_ERROR = " << err << " " << std::endl;
}