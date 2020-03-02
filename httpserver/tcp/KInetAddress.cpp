#include "KInetAddress.h"

#include <strings.h>
#include <netinet/in.h>

#include "KSocketsOps.h"
#include "../utils/KTypes.h"

// /* Structure describing an Internet socket address.  */
// struct sockaddr_in {
//     sa_family_t    sin_family; /* address family: AF_INET */
//     uint16_t       sin_port;   /* port in network byte order */
//     struct in_addr sin_addr;   /* internet address */
// };

// /* Internet address. */
// typedef uint32_t in_addr_t;
// struct in_addr {
//     in_addr_t       s_addr;     /* address in network byte order */
// };

using namespace kback;

static const in_addr_t kInaddrAny = INADDR_ANY;

// static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6),
//               "InetAddress is same size as sockaddr_in6");

InetAddress::InetAddress(uint16_t port)
{
    memZero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = sockets::hostToNetwork32(kInaddrAny);
    addr_.sin_port = sockets::hostToNetwork16(port);
}

InetAddress::InetAddress(const std::string &ip, uint16_t port)
{
    memZero(&addr_, sizeof addr_);
    sockets::fromHostPort(ip.c_str(), port, &addr_);
}

std::string InetAddress::toHostPort() const
{
    char buf[32];
    sockets::toHostPort(buf, sizeof buf, addr_);
    return buf;
}