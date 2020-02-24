// epoll LT 程序

#include <iostream>
#include "hpserver/KInetAddress.h"
#include "hpserver/KSocket.h"
#include "hpserver/KBuffer.h"
#include <sys/epoll.h>
#include <string.h>
#include <vector>
#include <unistd.h>

using namespace kb;

int main()
{
    int sockfd = createTcpSocket();
    InetAddress localaddr(9981);

    Socket server(sockfd);
    server.bindAddress(localaddr);
    server.listen();

    // 储存客户端地址和fd
    int clntfd = -1;
    InetAddress peeraddr(0);

    //
    int epfd = epoll_create1(EPOLL_CLOEXEC);

    struct epoll_event event;
    memset(&event, 0, sizeof event);

    // 然后设置所要关注事件的参数
    event.data.fd = sockfd;
    event.events = EPOLLIN;
    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event) < 0)
    {
        std::cerr << "epoll_ctl error" << std::endl;
    }

    // 保存epoll_wait调用后的活动事件
    std::vector<struct epoll_event> events(16);

    const int timeoutMs = 1000;

    Buffer buffer_;
    for (;;)
    {
        int numEvents = ::epoll_wait(epfd, events.data(), events.size(), timeoutMs);
        if (numEvents > 0)
        {
            std::cout << numEvents << " events happended" << std::endl;

            if (numEvents == events.size())
            {
                events.resize(events.size() * 2);
            }

            for (int i = 0; i < numEvents; ++i)
            {
                if (events[i].data.fd == sockfd)
                {
                    clntfd = server.accept(&peeraddr);
                    event.data.fd = clntfd;
                    event.events = EPOLLIN;
                    if (::epoll_ctl(epfd, EPOLL_CTL_ADD, clntfd, &event) < 0)
                    {
                        std::cerr << "epoll_ctl error" << std::endl;
                    }
                    std::cout << "connect client " << clntfd << std::endl;
                }
                else if(events[i].data.fd >= 0 && events[i].data.fd == clntfd)
                {
                    // 处理client即可
                    int saveErrno = 0;
                    int str_len = buffer_.readFd(events[i].data.fd, &saveErrno);
                    if(str_len == 0)
                    {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, events[i].data.fd, nullptr);
                        close(events[i].data.fd);
                        clntfd = -1;
                        std::cout << "close client " << events[i].data.fd << std::endl;
                    }
                    else
                    {
                        // echo事件
                        int nw = ::write(events[i].data.fd, buffer_.peek(), buffer_.readableBytes());
                        if(nw > 0)
                        {
                            buffer_.retrieve(nw);
                        }
                        std::cout << "write data " << events[i].data.fd << std::endl;
                    }
                }
                
            }
        }
        else if (numEvents == 0)
        {
            std::cout << "nothing happened" << std::endl;
        }
        else
        {
            std::cout << "Error: epoll_wait" << std::endl;
        }
    }
    close(epfd);
    return 0;
}
