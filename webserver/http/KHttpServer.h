#pragma once
#include "KStaticFileCache.h"
#include "webserver/tcp/KTcpServer.h"
#include "webserver/utils/Knoncopyable.h"
#include <sys/stat.h>

namespace kback {

class HttpRequest;
class HttpResponse;

class HttpServer : noncopyable {
public:
  typedef std::function<void(const HttpRequest &, HttpResponse *)> HttpCallback;

  HttpServer(EventLoop *loop, const InetAddress &listenAddr,
             const string &name);

  EventLoop *getLoop() const { return server_.getLoop(); }

  void setHttpCallback(const HttpCallback &cb) { httpCallback_ = cb; }

  void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
  void setStaticFileRoot(string root);

  void start();

private:
  void onConnection(const TcpConnectionPtr &conn);
  void onMessage(const TcpConnectionPtr &conn, Buffer *buf,
                 Timestamp receiveTime);
  void onRequest(const TcpConnectionPtr &, const HttpRequest &);
  void sendResponse(const TcpConnectionPtr &conn, const HttpResponse &response,
                    int fileFd = -1, size_t fileSize = 0);

  TcpServer server_;
  HttpCallback httpCallback_;
  string staticFileRoot_;
  StaticFileCache staticFileCache_;
};

} // namespace kback
