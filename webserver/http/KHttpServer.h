#pragma once
#include "../tcp/KTcpServer.h"
#include "../utils/Knoncopyable.h"
#include <sys/stat.h>

namespace kback
{

class HttpRequest;
class HttpResponse;

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