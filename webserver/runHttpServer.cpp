#include "http/KHttpServer.h"
#include "http/KHttpRequest.h"
#include "http/KHttpResponse.h"
#include "http/KIcons.h"
#include "loop/KEventLoop.h"

#include <iostream>
#include <map>

using namespace kback;

extern char favicon[555];
bool benchmark = false;

void onRequest(const HttpRequest &req, HttpResponse *resp)
{
#ifdef USE_STD_COUT
    std::cout << "Headers " << req.methodString() << " " << req.path() << std::endl;
#endif
    if (!benchmark)
    {
        const std::map<string, string> &headers = req.headers();
        for (const auto &header : headers)
        {
#ifdef USE_STD_COUT

            std::cout << header.first << ": " << header.second << std::endl;
#endif
        }
    }

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

int main(int argc, char *argv[])
{
    int numThreads = 0;
    if (argc > 1)
    {
        benchmark = true;
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