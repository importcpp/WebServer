#pragma once

#include "../utils/Kcopyable.h"
#include <string>
#include <netinet/in.h>

namespace kback
{
///
/// Wrapper of sockaddr_in.
/// POD: plain old data structure
/// This is an POD interface class.
class InetAddress : public copyable
{
public:
    // Constructs an endpoint with given port number.
    // 经常使用在 TcpServer listening上
    explicit InetAddress(uint16_t port);

    /// Constructs an endpoint with given ip and port.
    /// @c ip should be "1.2.3.4"
    InetAddress(const std::string &ip, uint16_t port);

    /// Constructs an endpoint with given struct @c sockaddr_in
    /// Mostly used when accepting new connections
    InetAddress(const struct sockaddr_in &addr)
        : addr_(addr)
    {
    }

    std::string toHostPort() const;

    // default copy/assignment are Okay
    const struct sockaddr_in &getSockAddrInet() const { return addr_; }
    void setSockAddrInet(const struct sockaddr_in &addr) { addr_ = addr; }

private:
    struct sockaddr_in addr_;
};
} // namespace kback