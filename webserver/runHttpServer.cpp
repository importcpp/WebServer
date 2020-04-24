#include "http/KHttpServer.h"
#include "http/KHttpRequest.h"
#include "http/KHttpResponse.h"
#include "http/KIcons.h"
#include "loop/KEventLoop.h"

#include <iostream>
#include <map>

using namespace kback;

extern char favicon[555];

void onRequest(const HttpRequest &req, HttpResponse *resp)
{
    if (req.path() == "/good")
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

int main(int argc, char *argv[])
{
    int numThreads = 0;
    if (argc > 1)
    {
        numThreads = atoi(argv[1]);
    }
    EventLoop loop;
    HttpServer server(&loop, InetAddress(8888), "httpserver");
    server.setHttpCallback(onRequest);
    server.setThreadNum(numThreads);
    server.start();
    loop.loop();

    return 0;
}