#include "http/KHttpServer.h"
#include "http/KHttpRequest.h"
#include "http/KHttpResponse.h"
#include "loop/KEventLoop.h"
#include <iostream>
#include <map>

using namespace kback;

int main(int argc, char *argv[])
{
    int numThreads = 3;
    if (argc > 1)
    {
        numThreads = atoi(argv[1]);
    }
    EventLoop loop;
    HttpServer server(&loop, InetAddress(8888), "httpserver");
    server.setThreadNum(numThreads);
    server.start();
    loop.loop();

    return 0;
}