#include <iostream>
#include <functional>

#include "hpserver/KInetAddress.h"
#include "hpserver/KSocket.h"
#include "hpserver/KBuffer.h"

#include "thread/KThreadPool.h"

// for http
#include "http/KHttpContext.h"
#include "http/KHttpRequest.h"
#include "http/KHttpResponse.h"

#include <sys/epoll.h>
#include <string.h>
#include <vector>
#include <unistd.h>
#include <assert.h>

#include "utils/KIcon.h"

using namespace kb;

/// Http server 版本-1 using LT ///

const int timeoutMs = 5000;

void selectResponse(const HttpRequest &req, HttpResponse *resp)
{
    if (req.path() == "/")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->addHeader("Server", "Muduo");
        string now = Timestamp::now().toFormattedString();
        resp->setBody("<html><head><title>This is title</title></head>"
                      "<body><h1>Hello</h1>Now is " +
                      now +
                      "</body></html>");
    }
    else if (req.path() == "/favicon.ico")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("image/png");
        resp->setBody(string(favicon, sizeof favicon));
    }
    else if (req.path() == "/hello")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->addHeader("Server", "Muduo");
        resp->setBody("hello, world!\n");
    }
    else
    {
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setCloseConnection(true);
    }
}

void httpOnRequest(int epfd, int clntfd)
{
    std::cout << "tid: " << std::this_thread::get_id()
              << std::endl;
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
        // // echo事件
        // int nw = ::write(clntfd, buffer_.peek(), buffer_.readableBytes());
        // if (nw > 0)
        // {
        //     buffer_.retrieve(nw);
        // }
        // std::cout << "write data " << clntfd << std::endl;
        // 处理http请求
        HttpContext *context = new HttpContext();
        Timestamp receiveTime(Timestamp::now());
        if (!context->parseRequest(&buffer_, receiveTime))
        {
            string badReq("HTTP/1.1 400 Bad Request\r\n\r\n");
            int nwrote = ::write(clntfd, badReq.data(), badReq.size());
            assert(nwrote == badReq.size());
        }

        if (context->gotAll())
        {
            HttpRequest req = context->request();

            const string &connection = req.getHeader("Connection");
            // 判断是否是长连接
            bool close = connection == "close" ||
                         (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
            HttpResponse response(close);

            selectResponse(req, &response);
            Buffer buf;
            response.appendToBuffer(&buf);
            string temp = buf.retrieveAsString();
            int nwrote = ::write(clntfd, temp.data(), temp.size());
            assert(nwrote == temp.size());
            if (response.closeConnection())
            {
                //
            }
            context->reset();
        }
    }
}

int main()
{

    // 添加线程池 来处理http请求
    ThreadPool pool("Test");
    pool.start(5);

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
                    std::cout << "assign task to client " << clntfd << std::endl;
                    pool.run(std::bind(httpOnRequest, epfd, clntfd));
                    // httpOnRequest(epfd, clntfd);
                    // 让新的线程处理读取 然后在 response
                    // 版本一设定： 每次触发就是当作一个短期task，执行玩后就退出
                    // 版本二设定： 利用map 来维护fd和thread的对应关系，thread只有在fd销毁时才算完成任务，否则一直等待
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
