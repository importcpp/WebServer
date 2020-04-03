#include "KSocketsOps.h"

#include "../utils/KTypes.h"

#include <fcntl.h>
#include <iostream>

#include <errno.h>
#include <unistd.h> //

using namespace kback;

namespace
{
typedef struct sockaddr SA;

// 这里是将sockaddr_in 转化为 sockaddr
const SA *sockaddr_cast(const struct sockaddr_in *addr)
{
    return static_cast<const SA *>(implicit_cast<const void *>(addr));
}

SA *sockaddr_cast(struct sockaddr_in *addr)
{
    return static_cast<SA *>(implicit_cast<void *>(addr));
}

void setNonBlockAndCloseOnExec(int sockfd)
{
    // non-block
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);

    // close-on-exec
    flags = ::fcntl(sockfd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    ret = ::fcntl(sockfd, F_SETFD, flags);
}

} // namespace

int sockets::createNonblockingOrDie()
{
    int sockfd = ::socket(AF_INET,
                          SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                          IPPROTO_TCP);
    if (sockfd < 0)
    {
        std::cout << "LOG_SYSFATAL:   "
                  << "sockets::createNonblockingOrDie" << std::endl;
    }
    return sockfd;
}

int sockets::connect(int sockfd, const struct sockaddr_in &addr)
{
    return ::connect(sockfd, sockaddr_cast(&addr), sizeof addr);
}

void sockets::bindOrDie(int sockfd, const struct sockaddr_in &addr)
{
    int ret = ::bind(sockfd, sockaddr_cast(&addr), sizeof addr);
    if (ret < 0)
    {
        std::cout << "LOG_SYSFATAL:   "
                  << "sockets::bindOrDie" << std::endl;
    }
}

void sockets::listenOrDie(int sockfd)
{
    int ret = ::listen(sockfd, SOMAXCONN);
    if (ret < 0)
    {
        std::cout << "LOG_SYSFATAL:   "
                  << "sockets::listenOrDie" << std::endl;
    }
}

int sockets::accept(int sockfd, struct sockaddr_in *addr)
{
    socklen_t addrlen = sizeof *addr;
#if VALGRIND
    int connfd = ::accept(sockfd, sockaddr_cast(addr), &addrlen);
    setNonBlockAndCloseOnExec(connfd);
#else
    int connfd = ::accept4(sockfd, sockaddr_cast(addr),
                           &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    if (connfd < 0)
    {
        int savedErrno = errno;
        std::cout << "LOG_SYSERR:   "
                  << "Socket::accept" << std::endl;
        switch (savedErrno)
        {
        case EAGAIN:
        case ECONNABORTED:
        case EINTR:
        case EPROTO: 
        case EPERM:
        case EMFILE: // per-process lmit of open file desctiptor ???
            // expected errors
            errno = savedErrno;
            break;
        case EBADF:
        case EFAULT:
        case EINVAL:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case ENOTSOCK:
        case EOPNOTSUPP:
            // unexpected errors
            std::cout << "LOG_FATAL:   "
                      << "unexpected error of ::accept " << std::endl;
            break;
        default:
            std::cout << "LOG_FATAL:   "
                      << "unknown error of ::accept" << std::endl;
            break;
        }
    }
    return connfd;
}

void sockets::close(int sockfd)
{
    if (::close(sockfd) < 0)
    {
        std::cout << "LOG_SYSERR:   "
                  << "sockets::close" << std::endl;
    }
}

void sockets::shutdownWrite(int sockfd)
{
    if (::shutdown(sockfd, SHUT_WR) < 0)
    {
        std::cout << "LOG_SYSERR:   "
                  << "sockets::shutdownWrite" << std::endl;
    }
}

void sockets::toHostPort(char *buf, size_t size,
                         const struct sockaddr_in &addr)
{
    char host[INET_ADDRSTRLEN] = "INVALID";
    ::inet_ntop(AF_INET, &addr.sin_addr, host, sizeof host);
    uint16_t port = sockets::networkToHost16(addr.sin_port);
    snprintf(buf, size, "%s:%u", host, port);
}

void sockets::fromHostPort(const char *ip, uint16_t port,
                           struct sockaddr_in *addr)
{
    addr->sin_family = AF_INET;
    addr->sin_port = hostToNetwork16(port);
    if (::inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
    {
        std::cout << "LOG_SYSERR:   "
                  << "sockets::fromHostPort" << std::endl;
    }
}

struct sockaddr_in sockets::getLocalAddr(int sockfd)
{
    struct sockaddr_in localaddr;
    memZero(&localaddr, sizeof localaddr);
    socklen_t addrlen = sizeof(localaddr);
    if (::getsockname(sockfd, sockaddr_cast(&localaddr), &addrlen) < 0)
    {
        std::cout << "LOG_SYSERR:   "
                  << "sockets::getLocalAddr" << std::endl;
    }
    return localaddr;
}  

struct sockaddr_in sockets::getPeerAddr(int sockfd)
{
    struct sockaddr_in peeraddr;
    memZero(&peeraddr, sizeof peeraddr);
    socklen_t addrlen = sizeof(peeraddr);
    // 
    if (::getpeername(sockfd, sockaddr_cast(&peeraddr), &addrlen) < 0)
    {
        std::cout << "LOG_SYSERR:   "
                  << "sockets::getPeerAddr" << std::endl;
    }
    return peeraddr;
}

int sockets::getSocketError(int sockfd)
{
    int optval;
    socklen_t optlen = sizeof optval;

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        return errno;
    }
    else
    {
        return optval;
    }
}

bool sockets::isSelfConnect(int sockfd)
{
    struct sockaddr_in localaddr = getLocalAddr(sockfd);
    struct sockaddr_in peeraddr = getPeerAddr(sockfd);
    return localaddr.sin_port == peeraddr.sin_port && localaddr.sin_addr.s_addr == peeraddr.sin_addr.s_addr;
}