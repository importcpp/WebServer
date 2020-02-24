#include "KSocket.h"
#include <unistd.h> // close
#include "KInetAddress.h"
#include <string.h> // memset

using namespace kb;

int kb::createTcpSocket()
{
    int sockfd = ::socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0)
    {
        std::cout << "LOG_SYSFATAL:   "
                  << "Socket::createTcpSocket" << std::endl;
    }
    return sockfd;
}

Socket::~Socket()
{
    if (::close(sockfd_) < 0)
    {
        std::cout << "LOG_SYSERR:   "
                  << "Socket::close" << std::endl;
    }
}

void Socket::bindAddress(const InetAddress &localaddr)
{
    struct sockaddr_in myaddr = localaddr.getSockAddrInet();
    int ret = ::bind(sockfd_, (sockaddr *)&myaddr, sizeof myaddr);
    if (ret < 0)
    {
        std::cout << "LOG_SYSFATAL:   "
                  << "Socket::bindAddress" << std::endl;
    }
}

void Socket::listen()
{
    int ret = ::listen(sockfd_, SOMAXCONN);
    if (ret < 0)
    {
        std::cout << "LOG_SYSFATAL:   "
                  << "Socket::listen" << std::endl;
    }
}

// 完成accpet
int Socket::accept(InetAddress *peeraddr)
{
    struct sockaddr_in caddr; // client addr
    socklen_t caddr_len = sizeof caddr;
    memset(&caddr, 0, caddr_len);
    int connfd = ::accept(sockfd_, (struct sockaddr *)&caddr, &caddr_len);
    if (connfd < 0)
    {
        std::cout << "LOG_SYSERR:   " << "Socket::accept" << std::endl;
    }
    if(connfd >= 0)
    {
        peeraddr->setSockAddrInet(caddr);
    }
    return connfd;
}