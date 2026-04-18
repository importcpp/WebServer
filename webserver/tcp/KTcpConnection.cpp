#include "KTcpConnection.h"

#include "webserver/loop/KEventLoop.h"
#include "webserver/poller/KChannel.h"
#include "webserver/utils/KAsyncLogger.h"
#include "webserver/utils/KTypes.h"
#include "KTcpIoMode.h"
#include "KSocket.h"
#include "KSocketsOps.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

using namespace kback;

void DisabledTcpConnectionLifecycle::afterConnectDestroyed(
    TcpConnection &connection) const {
  (void)connection;
}

void EnabledTcpConnectionLifecycle::afterConnectDestroyed(
    TcpConnection &connection) const {
  connection.releaseForRecycle();
  connection.enqueueForRecycle();
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg,
                             int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : state_(kConnecting), loop_(CheckNotNull<EventLoop>(loop)),
      name_(nameArg), socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr), peerAddr_(peerAddr), sendFileFd_(-1),
      sendFileOffset_(0), sendFileRemaining_(0) {
  KBACK_LOG_DEBUG("TcpConnection::ctor[%s] at %p fd=%d", name_.c_str(), this,
                  sockfd);
  channel_->setReadCallback(
      [this](Timestamp receiveTime) { handleRead(receiveTime); });
  channel_->setWriteCallback([this] { handleWrite(); });
  channel_->setCloseCallback([this] { handleClose(); });
  channel_->setErrorCallback([this] { handleError(); });
}

TcpConnection::~TcpConnection() {
  if (sendFileFd_ >= 0) {
    ::close(sendFileFd_);
  }
  KBACK_LOG_DEBUG("TcpConnection::dtor[%s] at %p fd=%d", name_.c_str(), this,
                  channel_->fd());
}

void TcpConnection::setNewTcpConnection(EventLoop *loop,
                                        const std::string &nameArg, int sockfd,
                                        const InetAddress &localAddr,
                                        const InetAddress &peeAddr) {
  loop_ = CheckNotNull<EventLoop>(loop);
  name_ = nameArg;
  state_ = kConnecting;
  socket_.reset(new Socket(sockfd));
  channel_.reset(new Channel(loop_, sockfd));
  localAddr_ = localAddr;
  peerAddr_ = peeAddr;
  inputBuffer_.retrieveAll();
  outputBuffer_.retrieveAll();
  resetSendFileState();

  KBACK_LOG_DEBUG("TcpConnection::ctor[%s] at %p fd=%d", name_.c_str(), this,
                  sockfd);

  channel_->setReadCallback(
      [this](Timestamp receiveTime) { handleRead(receiveTime); });
  channel_->setWriteCallback([this] { handleWrite(); });
  channel_->setCloseCallback([this] { handleClose(); });
  channel_->setErrorCallback([this] { handleError(); });
  context_.reset();
}

void TcpConnection::send(const std::string &message) {
  send(std::string_view(message));
}

void TcpConnection::send(std::string &&message) {
  if (state_ != kConnected) {
    return;
  }

  if (loop_->isInLoopThread()) {
    sendInLoop(message);
    return;
  }

  auto self = shared_from_this();
  loop_->runInLoop([self, payload = std::move(message)]() mutable {
    self->sendInLoop(payload);
  });
}

void TcpConnection::send(const char *message) {
  if (message == nullptr) {
    return;
  }
  send(std::string_view(message));
}

void TcpConnection::send(std::string_view message) {
  if (state_ == kConnected) {
    if (loop_->isInLoopThread()) {
      sendInLoop(message);
    } else {
      auto self = shared_from_this();
      std::string payload(message);
      loop_->runInLoop([self, payload = std::move(payload)] {
        self->sendInLoop(payload);
      });
    }
  }
}

void TcpConnection::send(const void *data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  send(std::string_view(static_cast<const char *>(data), len));
}

void TcpConnection::send(Buffer *buffer) {
  if (buffer == nullptr) {
    return;
  }

  const size_t readable = buffer->readableBytes();
  if (readable == 0) {
    return;
  }

  if (loop_->isInLoopThread() && buffer->isReadableContiguous()) {
    const std::string_view payload = buffer->readableView();
    sendInLoop(payload);
    buffer->retrieve(payload.size());
    return;
  }

  send(buffer->retrieveAsString());
}

// sendInLoop() 会先尝试直接发送数据，如果一次发送完毕，就不会启用writeCallback
// 如果只发送了部分数据，则把剩余的数据放入outputBuffer_,
// 并开始关注writable事件，
// 以后在handlewrite中发送剩余的数据， 如果当前outputBuffer_已经有待发送的数据，
// 那么就不能先尝试发送了，因为会造成数据乱序
void TcpConnection::sendInLoop(std::string_view message) {
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = message.size();
  // 先考虑outputbuffer里面是否含有缓冲，没有的话，那么可以直接写进输出buffer
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = tcp_io_mode::writeDirect(channel_->fd(), message);
    if (nwrote >= 0) {
      remaining -= static_cast<size_t>(nwrote);
      if (remaining == 0 && writeCompleteCallback_) {
        auto self = shared_from_this();
        loop_->queueInLoop([self, callback = writeCompleteCallback_] {
          callback(self);
        });
      }
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        KBACK_LOG_SYSERR("TcpConnection::sendInLoop");
      }
    }
  }
  assert(nwrote >= 0);
  if (remaining > 0) {
    outputBuffer_.append(message.data() + nwrote, remaining);
    // 如果没有关注writable事件，则开始关注
    if (!channel_->isWriting()) {
      channel_->enableWriting();
    }
  }
}

void TcpConnection::sendAllOneTimeInLoop(const std::string &message) {
  sendInLoop(message);
}

void TcpConnection::shutdown() {
  if (state_ == kConnected) {
    setState(kDisconnecting);
    auto self = shared_from_this();
    loop_->runInLoop([self] { self->shutdownInLoop(); });
  }
}

// 没有直接关闭TCP connection, 数据写完成后，只关闭写
void TcpConnection::shutdownInLoop() {
  loop_->assertInLoopThread();
  if (!channel_->isWriting()) {
    // 优雅的关闭套接字
    socket_->shutdownWrite();
  }
}

// 禁用Nagle算法，避免连续发包出现的延迟，这对编写低延迟网络服务很重要
void TcpConnection::setTcpNoDelay(bool on) { socket_->setTcpNoDelay(on); }

// tcp connection 处理连接建立的过程
// 1. 利用state_变量 标志 连接的状态
// 2. 掉用channel_->enableReading()
// 将channel负责的文件描述符中的可读事件注册到loop中
//    并用IO复用机制poller类来监视文件描述符
// 3. 最后调用connectioncallback函数 （注意其中使用的shared_from_this()）
void TcpConnection::connectEstablished() {
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  socket_->setKeepAlive(true);
  channel_->enableEpollET();
  channel_->enableReading();
  connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
  loop_->assertInLoopThread();
  if (state_ != kDisconnected) {
    setState(kDisconnected);
  }
  resetSendFileState();
  channel_->disableAll();
  connectionCallback_(shared_from_this());

  // 移除poller对channel_指针的管理
  loop_->removeChannel(get_pointer(channel_));
  recycleLifecycle_.afterConnectDestroyed(*this);
}

void TcpConnection::releaseForRecycle() {
  socket_.reset();
  channel_.reset();
  context_.reset();
}

void TcpConnection::enqueueForRecycle() {
  if (recycleCallback_) {
    recycleCallback_(shared_from_this());
  }
}

void TcpConnection::handleRead(Timestamp receiveTime) {
  int savedErrno = 0;
  ssize_t n = tcp_io_mode::read(inputBuffer_, channel_->fd(), &savedErrno);
  if (n > 0) {
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  } else if (n == 0) {
    handleClose();
  } else {
    errno = savedErrno;
    KBACK_LOG_SYSERR("TcpConnection::handleRead");
    handleError();
  }
}

void TcpConnection::handleWrite() {
  loop_->assertInLoopThread();
  if (channel_->isWriting()) {
    if (outputBuffer_.readableBytes() > 0) {
      int savedErrno = 0;
      ssize_t n =
          tcp_io_mode::write(outputBuffer_, channel_->fd(), &savedErrno);
      if (n < 0 && savedErrno != EAGAIN && savedErrno != EWOULDBLOCK) {
        errno = savedErrno;
        KBACK_LOG_SYSERR("TcpConnection::handleWrite");
        handleError();
        return;
      }
    }

    if (outputBuffer_.readableBytes() == 0 && sendFileRemaining_ > 0) {
      sendFileInLoop();
    }

    if (outputBuffer_.readableBytes() == 0 && sendFileRemaining_ == 0) {
      maybeCompleteWrite();
    }
  } else {
    KBACK_LOG_TRACE("Connection is down, no more writing");
  }
}

// handleclose 的工作
// 1. 取消channel中设置的关注事件
// 2. 利用回调(将移除工作放到loop中)完成TcpConnection的注销工作
//    2.1 移除TcpServer的map中TcpConnection中的管理
//    2.2 再掉用自身的
void TcpConnection::handleClose() {
  loop_->assertInLoopThread();
  KBACK_LOG_TRACE("TcpConnection::handleClose state=%d",
                  static_cast<int>(state_));
  assert(state_ == kConnected || state_ == kDisconnecting);
  setState(kDisconnected);
  resetSendFileState();
  // 这里不采取关闭fd的方式，因为建立了RAII的socket对象用于管理fd的析构
  // 这里的关闭是处理完输入输出流，同样的这里不直接关闭也可以方便找出程序的漏洞
  channel_->disableAll();
  closeCallback_(shared_from_this());
}

void TcpConnection::handleError() {
  int err = sockets::getSocketError(channel_->fd());
  (void)err;
  KBACK_LOG_ERROR("TcpConnection::handleError [%s] - SO_ERROR=%d",
                  name_.c_str(), err);
}

void TcpConnection::sendFileInLoop() {
  loop_->assertInLoopThread();
  while (sendFileRemaining_ > 0) {
    ssize_t n = ::sendfile(channel_->fd(), sendFileFd_, &sendFileOffset_,
                           sendFileRemaining_);
    if (n > 0) {
      sendFileRemaining_ -= static_cast<size_t>(n);
      continue;
    }
    if (n == 0) {
      resetSendFileState();
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }

    KBACK_LOG_SYSERR("TcpConnection::sendFileInLoop");
    handleError();
    resetSendFileState();
    return;
  }

  if (sendFileRemaining_ == 0) {
    resetSendFileState();
  }
}

void TcpConnection::maybeCompleteWrite() {
  if (channel_->isWriting()) {
    channel_->disableWriting();
  }

  if (writeCompleteCallback_) {
    auto self = shared_from_this();
    loop_->queueInLoop([self, callback = writeCompleteCallback_] {
      callback(self);
    });
  }

  if (state_ == kDisconnecting) {
    shutdownInLoop();
  }
}

void TcpConnection::resetSendFileState() {
  if (sendFileFd_ >= 0) {
    ::close(sendFileFd_);
  }
  sendFileFd_ = -1;
  sendFileOffset_ = 0;
  sendFileRemaining_ = 0;
}

void TcpConnection::hpSendFile(int srcFd, size_t count) {
  if (srcFd < 0 || count == 0) {
    if (srcFd >= 0) {
      ::close(srcFd);
    }
    return;
  }

  if (state_ != kConnected) {
    ::close(srcFd);
    return;
  }

  if (!loop_->isInLoopThread()) {
    auto self = shared_from_this();
    loop_->runInLoop([self, srcFd, count] { self->hpSendFile(srcFd, count); });
    return;
  }

  if (sendFileFd_ >= 0) {
    ::close(srcFd);
    return;
  }

  sendFileFd_ = srcFd;
  sendFileOffset_ = 0;
  sendFileRemaining_ = count;

  if (outputBuffer_.readableBytes() == 0) {
    sendFileInLoop();
  }

  if (sendFileRemaining_ == 0 && outputBuffer_.readableBytes() == 0) {
    maybeCompleteWrite();
    return;
  }

  if (!channel_->isWriting()) {
    channel_->enableWriting();
  }
}
