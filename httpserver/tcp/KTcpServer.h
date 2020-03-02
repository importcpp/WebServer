#pragma once

#include "../utils/Knoncopyable.h"
#include "../utils/KCallbacks.h"
#include "KTcpConnection.h"
#include "KInetAddress.h"
#include <map>
#include <memory>
#include <string>

namespace kback
{
class Acceptor;
class EventLoop;
class EventLoopThreadPool;

// TcpServer class 的功能是管理accept 获得的TcpConnection
class TcpServer : noncopyable
{
public:
    TcpServer(EventLoop *loop, const InetAddress &listenAddr);
    TcpServer(EventLoop *loop, const InetAddress &listenAddr, const string &nameArg);
    ~TcpServer();

    const string &ipPort() const { return ipPort_; }
    const string &name() const { return name_; }

    // 启动server
    void start();

    EventLoop *getLoop() const { return loop_; }

    void setThreadNum(int numThreads);

    //
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

private:
    // not thread safe, but in loop
    void newConnection(int sockfd, const InetAddress &peerAddr);
    void removeConnection(const TcpConnectionPtr &conn);

    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

    EventLoop *loop_; // the acceptor loop
    const string ipPort_;
    const string name_;
    std::unique_ptr<Acceptor> acceptor_;
    ConnectionCallback connectionCallback_;

    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    bool started_;
    int nextConnId_; // always in loop thread
    ConnectionMap connections_;

    std::unique_ptr<EventLoopThreadPool> threadPool_;
};

} // namespace kback