#pragma once
#include "../tcp/KTcpServer.h"
#include "../utils/Knoncopyable.h"


namespace kback
{

class HttpRequest;
class HttpResponse;

/// A simple embeddable HTTP server designed for report status of a program.
/// It is not a fully HTTP 1.1 compliant server, but provides minimum features
/// that can communicate with HttpClient and Web browser.
/// It is synchronous, just like Java Servlet.
class HttpServer : noncopyable
{
public:
    typedef std::function<void(const HttpRequest &,
                               HttpResponse *)>
        HttpCallback;

    HttpServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const string &name);

    EventLoop *getLoop() const { return server_.getLoop(); }

    /// Not thread safe, callback be registered before calling start().
    void setHttpCallback(const HttpCallback &cb)
    {
        httpCallback_ = cb;
    }

    void setThreadNum(int numThreads)
    {
        server_.setThreadNum(numThreads);
    }

    void start();

private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp receiveTime);
    void onRequest(const TcpConnectionPtr &, const HttpRequest &);

    TcpServer server_;
    HttpCallback httpCallback_;
};

} // namespace kback