#include "KSocket.h"

#include "KInetAddress.h"
#include "KSocketsOps.h"

#include <netinet/in.h>
#include <netinet/tcp.h>

#include "../utils/KTypes.h" // memZero

using namespace kback;

Socket::~Socket()
{
  sockets::close(sockfd_);
}

void Socket::forceClose()
{
  sockets::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &addr)
{
  sockets::bindOrDie(sockfd_, addr.getSockAddrInet());
}

void Socket::listen()
{
  sockets::listenOrDie(sockfd_);
}

int Socket::accept(InetAddress *peeraddr)
{
  struct sockaddr_in addr;
  memZero(&addr, sizeof addr);
  int connfd = sockets::accept(sockfd_, &addr);
  if (connfd >= 0)
  {
    peeraddr->setSockAddrInet(addr);
  }
  return connfd;
}

// 设置调用close(socket)后,仍可继续重用该socket。
// 调用close(socket)一般不会立即关闭socket，而要经历TIME_WAIT的过程。
// BOOL bReuseaddr = TRUE;
// setsockopt( s, SOL_SOCKET, SO_REUSEADDR, ( const char* )&bReuseaddr, sizeof( BOOL ) )
void Socket::setReuseAddr(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof optval);
}

void Socket::shutdownWrite()
{
  sockets::shutdownWrite(sockfd_);
}

// 都是设置套接字的功能 setTcpNoDelay和setKeepAlive

void Socket::setTcpNoDelay(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
               &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
               &optval, static_cast<socklen_t>(sizeof optval));
}