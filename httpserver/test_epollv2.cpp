// 将client端的IO事件交由线程池来处理
#include <iostream>
#include <functional>
#include "hpserver/KInetAddress.h"
#include "hpserver/KSocket.h"
#include "hpserver/KBuffer.h"
#include "thread/KThreadPool.h"
#include <sys/epoll.h>
#include <string.h>
#include <vector>
#include <unistd.h>

using namespace kb;

const int timeoutMs = 5000;

void httpOnRequest(int epfd, int clntfd)
{
    // 全部委托给新的线程，注意销毁
    // 处理client即可
    Buffer buffer_;
    int saveErrno = 0;
    int str_len = buffer_.readFd(clntfd, &saveErrno);
    if (str_len == 0)
    {
        epoll_ctl(epfd, EPOLL_CTL_DEL, clntfd, NULL);
        close(clntfd);
        std::cout << "close client " << clntfd << std::endl;
        clntfd = -1;
        return;
    }
    else
    {
        // echo事件
        int nw = ::write(clntfd, buffer_.peek(), buffer_.readableBytes());
        if (nw > 0)
        {
            buffer_.retrieve(nw);
        }
        std::cout << "write data " << clntfd << std::endl;
        // 处理http请求
    }
}

int main()
{

    // 添加线程池 来处理http请求
    ThreadPool pool("Test");
    pool.start(1);

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

    for (;;)
    {
        int numEvents = ::epoll_wait(epfd, events.data(), events.size(), timeoutMs);

        // 事件分发处理
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
                else if (events[i].data.fd >= 0 && events[i].data.fd == clntfd)
                {
                    pool.run(std::bind(httpOnRequest, epfd, clntfd));
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
