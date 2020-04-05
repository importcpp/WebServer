#pragma once

#include "../utils/Knoncopyable.h"
#include <iostream>

namespace kback
{

class InetAddress;

// socket fd 的封装
// 在析构时关闭sockfd
// 线程安全，所有操作都转交给OS

class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {
        // std::cout << "sockfd_ is:  " << sockfd_ << std::endl;
    }

    ~Socket();

    int fd() const
    {
        return sockfd_;
    }

    // 服务端建立连接的三个流程
    void bindAddress(const InetAddress &localaddr);
    void listen();

    int accept(InetAddress *peeraddr);

    // 设置地址重用
    void setReuseAddr(bool on);

    void shutdownWrite();

    /// TCP 的一些操作，也要记住
    void setTcpNoDelay(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};

} // namespace kback