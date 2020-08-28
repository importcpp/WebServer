#pragma once

#include <arpa/inet.h>
#include <endian.h>

namespace kback {
namespace sockets {
inline uint64_t hostToNetwork64(uint64_t host64) { return htobe64(host64); }

inline uint32_t hostToNetwork32(uint32_t host32) { return htonl(host32); }

inline uint16_t hostToNetwork16(uint16_t host16) { return htons(host16); }

inline uint64_t networkToHost64(uint64_t net64) { return be64toh(net64); }

inline uint32_t networkToHost32(uint32_t net32) { return ntohl(net32); }

inline uint16_t networkToHost16(uint16_t net16) { return ntohs(net16); }

// 创建一个非阻塞socket文件描述符
int createNonblockingOrDie();

int connect(int sockfd, const struct sockaddr_in &addr);
void bindOrDie(int sockfd, const struct sockaddr_in &addr);
void listenOrDie(int sockfd);
int accept(int sockfd, struct sockaddr_in *addr);
void close(int sockfd);
void shutdownWrite(int sockfd);

void toHostPort(char *buf, size_t size, const struct sockaddr_in &addr);
void fromHostPort(const char *ip, uint16_t port, struct sockaddr_in *addr);

struct sockaddr_in getLocalAddr(int sockfd);
struct sockaddr_in getPeerAddr(int sockfd);

int getSocketError(int sockfd);
bool isSelfConnect(int sockfd);

} // namespace sockets

} // namespace kback
