#include "KHttpServer.h"
#include <iostream>
#include "../utils/KCallbacks.h"
#include "KHttpContext.h"
#include "KHttpRequest.h"
#include "KHttpResponse.h"
#include "KIcons.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

extern char favicon[555];

using namespace kback;

void defaultHttpCallback(const HttpRequest &req, HttpResponse *resp)
{
    // 根据请求内容设置相应报文
    if (req.path() == "/file")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->addHeader("Server", "Webserver");
        // 处理body留到之后的部分来处理
    }
    else if (req.path() == "/good")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->addHeader("Server", "Webserver");
        // 增大数据量，体现ringbuffer性能
        string AAA(950, '!');
        AAA += "\n";
        resp->setBody(AAA);
    }
    else if (req.path() == "/hello")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->addHeader("Server", "Webserver");
        resp->setBody("hello, world!\n");
    }
    else if (req.path() == "/")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/html");
        resp->addHeader("Server", "Webserver");
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
    else
    {
        resp->setStatusCode(HttpResponse::k404NotFound);
        resp->setStatusMessage("Not Found");
        resp->setCloseConnection(true);
    }
}

HttpServer::HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &name) : server_(loop, listenAddr, name),
                                             httpCallback_(defaultHttpCallback)
{
    server_.setConnectionCallback(std::bind(&HttpServer::onConnection, this, _1));
    server_.setMessageCallback(std::bind(&HttpServer::onMessage, this, _1, _2, _3));
}

void HttpServer::start()
{
#ifdef USE_STD_COUT
    std::cout << "LOG_WARN:   "
              << "HttpServer[" << server_.name()
              << "] starts listenning on " << server_.ipPort() << std::endl;
#endif
    // 启动TcpServer, 开始监听端口
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        conn->setContext(HttpContext());
    }
}

// 设置为TcpConnection的messageCallback_
void HttpServer::onMessage(const TcpConnectionPtr &conn,
                           Buffer *buf,
                           Timestamp receiveTime)
{
    HttpContext *context = boost::any_cast<HttpContext>(conn->getMutableContext());

    if (!context->parseRequest(buf, receiveTime))
    {
        conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
        conn->shutdown();
    }

    if (context->gotAll())
    {
        onRequest(conn, context->request());
        context->reset();
    }
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequest &req)
{
    const string &connection = req.getHeader("Connection");
    bool close_connection = connection == "close" ||
                 (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close_connection);
    httpCallback_(req, &response);

    if (req.path() == "/file")
    {

        // read file len
        size_t filesize = -1;
        struct stat statbuff;
        if (stat("./index.html", &statbuff) < 0)
        {
            // Error 404
            response.setStatusCode(HttpResponse::k404NotFound);
            response.setStatusMessage("Not Found");
            response.setCloseConnection(true);
            Buffer buf;
            response.appendToBuffer(&buf);
            string temp = buf.retrieveAsString();
            conn->send(temp);
        }
        else
        {
            filesize = statbuff.st_size;
            assert(filesize > 0);
            int srcFd = open("./index.html", O_RDONLY | O_NONBLOCK, 0);
            if(srcFd <= 0)
            {
                assert(srcFd > 0);
            }
            response.setFileSize(filesize);
            response.setSrcFd(srcFd);
            response.setStatusCode(HttpResponse::k200Ok);
            response.setStatusMessage("OK");
            response.setContentType("text/html");
            response.addHeader("Server", "Webserver");
            Buffer buf;
            response.appendToBuffer(&buf);
            string temp = buf.retrieveAsString();
            conn->send(temp);
            
            conn->hpSendFile(srcFd, filesize);
            close(srcFd);
        }
    }
    else
    {
        Buffer buf;
        response.appendToBuffer(&buf);
        // conn->send(&buf);
        string temp = buf.retrieveAsString();
        conn->send(temp);
    }

    if (response.closeConnection())
    {
        conn->shutdown();
    }
}