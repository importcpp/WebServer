#pragma once

#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>

#include <fcntl.h>

namespace kb
{

void setNonBlockAndCloseOnExec(int sockfd);
    
int createTcpSocket();

class InetAddress;

// 新建了一个Socket类，利用RAII方法将socket文件描述符对象的生命期严格绑定，
// 使得Tcp socket的创建的销毁更加简单
class Socket
{
private:
    const int sockfd_;

public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}
    ~Socket();

    int fd() const { return sockfd_; }

    // 服务端建立连接的三个流程
    void bindAddress(const InetAddress &localaddr);
    void listen();

    int accept(InetAddress *peeraddr);
    // Tcp的一些参数设置
};

} // namespace kb