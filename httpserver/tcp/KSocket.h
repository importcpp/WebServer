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
    /// abort if address in use
    void bindAddress(const InetAddress &localaddr);
    /// abort if address in use
    void listen();

    /// On success, returns a non-negative integer that is
    /// a descriptor for the accepted socket, which has been
    /// set to non-blocking and close-on-exec. *peeraddr is assigned.
    /// On error, -1 is returned, and *peeraddr is untouched.
    int accept(InetAddress *peeraddr);

    ///
    /// Enable/disable SO_REUSEADDR
    ///
    void setReuseAddr(bool on);

    void shutdownWrite();

    /// TCP 的一些操作，也要记住
    /// Enable/disable TCP_NODELAY (disable/enable Nagle's algorithm).
    ///
    void setTcpNoDelay(bool on);

    void setKeepAlive(bool on);

private:
    const int sockfd_;
};

} // namespace kback