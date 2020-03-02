#include "KHttpServer.h"
#include <iostream>
#include "../utils/KCallbacks.h"
#include "KHttpContext.h"
#include "KHttpRequest.h"
#include "KHttpResponse.h"

using namespace kback;

void defaultHttpCallback(const HttpRequest &, HttpResponse *resp)
{
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
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
    std::cout << "LOG_WARN:   "
              << "HttpServer[" << server_.name()
              << "] starts listenning on " << server_.ipPort() << std::endl;
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
    bool close = connection == "close" ||
                 (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    HttpResponse response(close);
    httpCallback_(req, &response);
    Buffer buf;
    response.appendToBuffer(&buf);
    // conn->send(&buf);
    string temp = buf.retrieveAsString();
    conn->send(temp); 
    if (response.closeConnection())
    {
        conn->shutdown();
    }
}