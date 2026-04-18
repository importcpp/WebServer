#include "KHttpServer.h"
#include "KHttpContext.h"
#include "KIcons.h"
#include "KHttpRequest.h"
#include "KHttpResponse.h"
#include "webserver/utils/KCallbacks.h"

#include <any>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char favicon[555];

using namespace kback;

void defaultHttpCallback(const HttpRequest &req, HttpResponse *resp) {
  // 根据请求内容设置相应报文
  if (req.path() == "/good") {
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/plain");
    resp->addHeader("Server", "Webserver");
    // 增大数据量，体现ringbuffer性能
    string AAA(950, '!');
    AAA += "\n";
    resp->setBody(AAA);
  } else if (req.path() == "/hello") {
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/plain");
    resp->addHeader("Server", "Webserver");
    resp->setBody("hello, world!\n");
  } else if (req.path() == "/") {
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("text/html");
    resp->addHeader("Server", "Webserver");
    string now = Timestamp::now().toFormattedString();
    resp->setBody("<html><head><title>This is title</title></head>"
                  "<body><h1>Hello</h1>Now is " +
                  now + "</body></html>");
  } else if (req.path() == "/favicon.ico") {
    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType("image/png");
    resp->setBody(string(favicon, sizeof favicon));
  } else {
    resp->setStatusCode(HttpResponse::k404NotFound);
    resp->setStatusMessage("Not Found");
    resp->setCloseConnection(true);
  }
}

HttpServer::HttpServer(EventLoop *loop, const InetAddress &listenAddr,
                       const string &name)
    : server_(loop, listenAddr, name), httpCallback_(defaultHttpCallback),
      staticFileRoot_(".") {
  staticFileCache_.setRoot(staticFileRoot_);
  server_.setConnectionCallback([this](const TcpConnectionPtr &conn) {
    onConnection(conn);
  });
  server_.setMessageCallback(
      [this](const TcpConnectionPtr &conn, Buffer *buf, Timestamp receiveTime) {
        onMessage(conn, buf, receiveTime);
      });
}

void HttpServer::setStaticFileRoot(string root) {
  staticFileRoot_ = root.empty() ? "." : std::move(root);
  staticFileCache_.setRoot(staticFileRoot_);
}

void HttpServer::start() {
#ifdef USE_STD_COUT
  std::cout << "LOG_WARN:   "
            << "HttpServer[" << server_.name() << "] starts listenning on "
            << server_.ipPort() << std::endl;
#endif
  // 启动TcpServer, 开始监听端口
  server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn) {
  if (conn->connected()) {
    conn->setContext(HttpContext());
  }
}

// 设置为TcpConnection的messageCallback_
void HttpServer::onMessage(const TcpConnectionPtr &conn, Buffer *buf,
                           Timestamp receiveTime) {
  HttpContext *context = std::any_cast<HttpContext>(conn->getMutableContext());
  assert(context != nullptr);

  if (!context->parseRequest(buf, receiveTime)) {
    conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
    conn->shutdown();
  }

  if (context->gotAll()) {
    onRequest(conn, context->request());
    context->reset();
  }
}

void HttpServer::onRequest(const TcpConnectionPtr &conn,
                           const HttpRequest &req) {
  const string &connection = req.getHeader("Connection");
  bool close_connection =
      connection == "close" ||
      (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
  if (req.path() == "/file") {
    auto entry = staticFileCache_.find("/index.html");
    HttpResponse response(close_connection);
    if (entry == nullptr) {
      response.setStatusCode(HttpResponse::k404NotFound);
      response.setStatusMessage("Not Found");
      response.setCloseConnection(true);
      sendResponse(conn, response);
    } else {
      const int fileFd = ::dup(entry->fileFd);
      if (fileFd < 0) {
        response.setStatusCode(HttpResponse::k500InternalServerError);
        response.setStatusMessage("Internal Server Error");
        response.setCloseConnection(true);
        sendResponse(conn, response);
        return;
      }
      response.setFileSize(entry->fileSize);
      response.setStatusCode(HttpResponse::k200Ok);
      response.setStatusMessage("OK");
      response.setContentType(entry->contentType);
      response.addHeader("Server", "Webserver");
      sendResponse(conn, response, fileFd, entry->fileSize);
    }
    return;
  }

  HttpResponse response(close_connection);
  httpCallback_(req, &response);
  sendResponse(conn, response);
}

void HttpServer::sendResponse(const TcpConnectionPtr &conn,
                              const HttpResponse &response, int fileFd,
                              size_t fileSize) {
  Buffer buffer;
  response.appendToBuffer(&buffer);
  conn->send(buffer.retrieveAsString());

  if (fileFd >= 0) {
    conn->hpSendFile(fileFd, fileSize);
  }

  if (response.closeConnection()) {
    conn->shutdown();
  }
}
