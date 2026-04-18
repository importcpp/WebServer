#include "webserver/http/KHttpRequest.h"
#include "webserver/http/KHttpResponse.h"
#include "webserver/http/KHttpServer.h"
#include "webserver/loop/KEventLoop.h"
#include <map>

using namespace kback;

int main(int argc, char *argv[]) {
  int numThreads = 3;
  if (argc > 1) {
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  HttpServer server(&loop, InetAddress(8888), "httpserver");
  server.setStaticFileRoot("webserver");
  server.setThreadNum(numThreads);
  server.start();
  loop.loop();
  return 0;
}
