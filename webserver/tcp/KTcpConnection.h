#pragma once
#include "../utils/KCallbacks.h"
#include "../utils/Knoncopyable.h"
#include "../utils/KTimestamp.h"
#ifdef USE_RINGBUFFER
#include "../tcp/KRingBuffer.h"
#else
#include "../tcp/KBuffer.h"
#endif
#include "KInetAddress.h"
#include <memory>
#include <string>
#include <boost/any.hpp>

namespace kback
{
class Channel;
class EventLoop;
class Socket;

// Tcp connection是唯一默认使用shared_ptr来管理的class, 也是唯一继承enable_shared_from_this的class
// 之所以要用shared_ptr是因为 tcpconnention的析构是由TcpServer来间接管理的，
// 也就是说tcpconnection被tcpserver所持有，
// 如果tcpconnection自己析构了而tcpserver不知道，将引发程序未定义的行为
// 所有用shared_ptr进行管理，使用引用计数功能，当引用计数为0时，即是最合理的析构的时刻
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                  const InetAddress &localAddr, const InetAddress &peerAddr);

    ~TcpConnection();

    void setNewTcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                             const InetAddress &localAddr, const InetAddress &peeAddr);

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

    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    // 
    void setRecycleCallback(const RecycleCallback & cb)
    {
        recycleCallback_ = cb;
    }

    // 只在TcpServer接受新连接的时候调用
    void connectEstablished();
    // 只在TcpServer移除连接的时候调用
    void connectDestroyed();

private:
    // 通过状态机来表示tcp的连接状态
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
    RecycleCallback recycleCallback_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
    boost::any context_;
};

} // namespace kback
