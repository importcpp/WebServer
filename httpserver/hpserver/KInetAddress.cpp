#include "KInetAddress.h"
#include <string.h> // memset

using namespace kb;


InetAddress::InetAddress(uint16_t port)
{
    memset(&addr_, 0, sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = INADDR_ANY;
    addr_.sin_port = htons(port);
}

