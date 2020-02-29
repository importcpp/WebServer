#pragma once

#include "KBuffer.h"
#include "KInetAddress.h"
#include "../utils/KCallbacks.h"
#include "../utils/Knoncopyable.h"
#include "../utils/KTimestamp.h"
#include <memory>
#include <string>
#include <boost/any.hpp>

namespace kback
{
class Channel;
class EventLoop;
class Socket;

// Tcp connection是唯一默认使用shared_ptr来管理的class, 也是唯一继承enable_shared_from_this的calss
// ??? 这源于其模糊的生命期
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                  const InetAddress &localAddr, const InetAddress &peerAddr);

    ~TcpConnection();

    EventLoop *getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress &localAddress() { return localAddr_; }
    const InetAddress &peerAddress() { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }

    void send(const std::string &message);
    // void send(Buffer *buf);
    void shutdown();
    void setTcpNoDelay(bool on);

    /// ============= Http ============ ///
    void setContext(const boost::any &context)
    {
        context_ = context;
    }

    const boost::any &getContext() const
    {
        return context_;
    }

    boost::any *getMutableContext()
    {
        return &context_;
    }
    /// ============= Http ============ ///

    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

    /// Internal use only.
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    // called when TcpServer accepts a new connection
    void connectEstablished(); // should be called only once
    // called when TcpServer has removed me from its map
    void connectDestroyed(); // should be called only once

private:
    enum StateE
    {
        kConnecting,
        kConnected,
        kDisconnecting,
        kDisconnected,
    };
    StateE state_;
    void setState(StateE s) { state_ = s; }
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const std::string &message);
    void shutdownInLoop();

    EventLoop *loop_;
    std::string name_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    InetAddress localAddr_;
    InetAddress peerAddr_;
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    Buffer inputBuffer_;
    Buffer outputBuffer_;

    boost::any context_;
};

} // namespace kback
