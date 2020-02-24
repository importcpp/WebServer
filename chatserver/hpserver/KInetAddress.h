#pragma once

#include <netinet/in.h>
#include <string>

namespace kb
{
class InetAddress
{
private:
    struct sockaddr_in addr_;

public:
    explicit InetAddress(uint16_t port);

    // InetAddress(const std::string &ip, uint16_t port);

    // InetAddress(const struct sockaddr_in &addr) : addr_(addr) {}

    // 获取或者设置 InetAddress 所管理的 网络地址和端口
    const struct sockaddr_in &getSockAddrInet() const { return addr_; }
    void setSockAddrInet(const struct sockaddr_in &addr) { addr_ = addr; }
};

} // namespace kb