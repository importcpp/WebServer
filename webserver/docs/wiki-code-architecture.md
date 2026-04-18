# WebServer Wiki: Code Architecture Walkthrough

## 1. 这份 Wiki 怎么用

这不是“再讲一遍架构说明”，而是一份源码导读地图。  
适合的使用场景是：

- 你第一次接手这个项目
- 你想知道应该从哪个文件开始看
- 你想带着问题去读代码，而不是从头到尾硬啃

最推荐的使用方式：

1. 打开一个源码文件
2. 对照本文“阅读路线”与“问题清单”
3. 一边看，一边在 IDE 里跳转

## 2. 第一张图：整个项目的阅读地图

```text
main: runHttpServer.cpp
  -> HttpServer
      -> TcpServer
          -> Acceptor
          -> TcpConnection
          -> EventLoopThreadPool
      -> HttpContext
          -> HttpRequest
      -> HttpResponse

TcpConnection
  -> Buffer
  -> Channel
      -> EventLoop
          -> EventManager
```

如果你只能记住一件事，请记住：

> `TcpConnection` 是单连接 I/O 核心，`EventLoop` 是线程内调度核心，`HttpContext` 是请求解析核心。

## 3. 新手阅读路线

### 第 1 站：先看程序怎么启动

文件：

- `webserver/runHttpServer.cpp`

你要回答的问题：

- 主线程先创建了什么？
- `HttpServer` 是怎么被挂到 `EventLoop` 上的？
- 为什么最后是 `loop.loop()` 而不是 `server.loop()`？

看完以后你应该知道：

- 这是一个以 `EventLoop` 为中心驱动的系统
- `HttpServer` 只是构建在 `TcpServer` 之上的应用层对象

### 第 2 站：看 HTTP 层怎么接入 TCP 层

文件：

- `webserver/http/KHttpServer.h`
- `webserver/http/KHttpServer.cpp`

重点方法：

- `HttpServer::HttpServer()`
- `HttpServer::onConnection()`
- `HttpServer::onMessage()`
- `HttpServer::onRequest()`

你要回答的问题：

- HTTP 层是如何把自己的逻辑挂到 TCP 层上的？
- 为什么 `HttpServer` 自己不直接收 fd，而是通过 `TcpConnection`？
- `HttpContext` 是在哪里创建、存放和重置的？

### 第 3 站：看 TCP 层总控

文件：

- `webserver/tcp/KTcpServer.h`
- `webserver/tcp/KTcpServer.cpp`

重点方法：

- `TcpServer::start()`
- `TcpServer::newConnection()`
- `TcpServer::removeConnection()`
- `TcpServer::removeConnectionInLoop()`

你要回答的问题：

- 新连接到来后为什么不是当前线程直接处理？
- `connections_` 这张 map 存的是什么？
- `TcpConnection` 为什么是 `shared_ptr`？
- `USE_RECYCLE` 为什么会让 `TcpServer` 变复杂？

### 第 4 站：看单连接 I/O 核心

文件：

- `webserver/tcp/KTcpConnection.h`
- `webserver/tcp/KTcpConnection.cpp`

重点方法：

- `connectEstablished()`
- `handleRead()`
- `sendInLoop()`
- `handleWrite()`
- `handleClose()`
- `connectDestroyed()`
- `hpSendFile()`

你要回答的问题：

- 一个连接对象从建立到销毁经历了哪些状态？
- 为什么写数据时先尝试直接写，而不是永远先进 output buffer？
- 为什么关闭连接要区分 `shutdown()` 和 `handleClose()`？

### 第 5 站：看事件循环内核

文件：

- `webserver/loop/KEventLoop.h`
- `webserver/loop/KEventLoop.cpp`

重点方法：

- `loop()`
- `runInLoop()`
- `queueInLoop()`
- `doPendingFunctors()`

你要回答的问题：

- `EventLoop` 为什么要保存 `threadId_`？
- 跨线程调用为什么不能直接操作 `Channel`？
- `pendingFunctors_` 的作用是什么？

### 第 6 站：看 epoll 分发骨架

文件：

- `webserver/poller/KEventManager.h`
- `webserver/poller/KEventManager.cpp`
- `webserver/poller/KChannel.h`
- `webserver/poller/KChannel.cpp`

你要回答的问题：

- `EventManager` 和 `Channel` 分别负责什么？
- 为什么 `Channel` 不拥有 fd？
- `revents_` 和 `events_` 分别表示什么？

### 第 7 站：最后再看 Buffer 和 HTTP 解析

文件：

- `webserver/tcp/KBuffer.*`
- `webserver/tcp/KRingBuffer.*`
- `webserver/http/KHttpContext.*`
- `webserver/http/KHttpRequest.h`
- `webserver/http/KHttpResponse.*`

你要回答的问题：

- 两种 Buffer 的区别是什么？
- 为什么 ringbuffer 模式下 HTTP 解析更复杂？
- `HttpContext` 为什么用状态机？

## 4. 如果你是带着“一个请求怎么跑完”这个问题来看的

最推荐按下面这个顺序追：

```text
runHttpServer.cpp
  -> HttpServer::start
  -> TcpServer::start
  -> Acceptor::listen
  -> Acceptor::handleRead
  -> TcpServer::newConnection
  -> TcpConnection::connectEstablished
  -> TcpConnection::handleRead
  -> HttpServer::onMessage
  -> HttpContext::parseRequest
  -> HttpServer::onRequest
  -> HttpResponse::appendToBuffer
  -> TcpConnection::send/sendInLoop
  -> TcpConnection::handleWrite
```

如果是 `/file` 路径，再加：

```text
HttpServer::onRequest
  -> open("./index.html")
  -> HttpResponse::appendToBuffer
  -> TcpConnection::sendAllOneTimeInLoop
  -> TcpConnection::hpSendFile
```

## 5. 如果你是带着“跨线程是怎么协调的”这个问题来看的

要看这几处：

- `KEventLoop::queueInLoop`
- `KAsyncWaker::wakeup`
- `KAsyncWaker::handleRead`
- `KEventLoop::doPendingFunctors`
- `KEventLoopThread::threadFunc`
- `KEventLoopThreadPool::getNextLoop`

理解方式：

1. `EventLoop` 只能在所属线程内安全操作。
2. 其他线程想让它干活，必须先把任务投进 `pendingFunctors_`。
3. 然后用 `eventfd` 把 loop 唤醒。
4. loop 醒来后执行 `doPendingFunctors()`。

这就是“线程安全 + 线程亲和性”同时成立的关键。

## 6. 如果你是带着“为什么会有这么多宏开关”这个问题来看的

先看这些宏：

- `USE_EPOLL_LT`
- `USE_RINGBUFFER`
- `USE_LOCKFREEQUEUE`
- `USE_SPINLOCK`
- `USE_RECYCLE`
- `USE_STD_COUT`

再观察这些文件：

- `webserver/tcp/KTcpConnection.cpp`
- `webserver/tcp/KAcceptor.cpp`
- `webserver/http/KHttpContext.cpp`
- `webserver/loop/KEventLoop.cpp`
- `webserver/tcp/KTcpServer.h`

你会发现当前工程的一个鲜明特点：

> 这个项目既是一个 WebServer，也是一个“多种并发/缓冲/IO 策略的实验场”。

这也是它很有学习价值的原因，但同时也是后续维护成本升高的来源。

## 7. 重点类速查表

| 类 | 所在文件 | 一句话职责 | 最应该先看的方法 |
| --- | --- | --- | --- |
| `EventLoop` | `loop/KEventLoop.*` | 线程内事件循环与回调调度核心 | `loop()`、`queueInLoop()` |
| `AsyncWaker` | `loop/KAsyncWaker.*` | 用 `eventfd` 唤醒阻塞中的 loop | `wakeup()`、`handleRead()` |
| `EventManager` | `poller/KEventManager.*` | epoll 封装层 | `poll()`、`updateChannel()` |
| `Channel` | `poller/KChannel.*` | fd 与回调之间的桥 | `handleEvent()` |
| `Acceptor` | `tcp/KAcceptor.*` | 监听 socket 与 accept 分发 | `listen()`、`handleRead()` |
| `TcpServer` | `tcp/KTcpServer.*` | 管理连接对象与 I/O 线程分发 | `newConnection()` |
| `TcpConnection` | `tcp/KTcpConnection.*` | 单连接读写与状态机核心 | `handleRead()`、`sendInLoop()` |
| `HttpServer` | `http/KHttpServer.*` | 在 TCP 层上挂接 HTTP 语义 | `onMessage()`、`onRequest()` |
| `HttpContext` | `http/KHttpContext.*` | HTTP 请求解析状态机 | `parseRequest()` |
| `Buffer` | `tcp/KBuffer.*` / `tcp/KRingBuffer.*` | 收发缓冲区 | `readFd*()`、`writeFd*()` |

## 8. 常见阅读误区

### 误区 1：以为 `HttpServer` 是主角

实际上 HTTP 层只是应用层外壳。  
真正驱动请求收发的是 `TcpConnection + EventLoop + Channel`。

### 误区 2：以为 `EventManager` 拥有 `Channel`

不是。  
`EventManager` 只是保存 `fd -> Channel*` 映射，生命周期管理仍然在上层对象。

### 误区 3：以为 `ThreadPool` 就是服务器线程池

`webserver/thread/KThreadPool.*` 更像实验/辅助实现。  
真正服务网络 I/O 的线程池是 `EventLoopThreadPool`。

### 误区 4：以为 `send()` 一定马上发出去

不一定。  
如果 socket 当前不可写，数据会先进 `outputBuffer_`，等下次可写事件再继续发送。

## 9. 调试时最值得打断点的地方

如果你要用 IDE 单步，推荐这些断点：

- `Acceptor::handleRead`
- `TcpServer::newConnection`
- `TcpConnection::connectEstablished`
- `TcpConnection::handleRead`
- `HttpServer::onMessage`
- `HttpContext::parseRequest`
- `HttpServer::onRequest`
- `TcpConnection::sendInLoop`
- `TcpConnection::handleWrite`
- `TcpConnection::handleClose`

如果你只打一个断点，建议先打在 `TcpConnection::handleRead()`。

## 10. 推荐的源码搜索命令

你可以直接在仓库根目录里用这些命令：

```bash
rg -n "newConnection|handleRead|handleWrite|connectDestroyed" webserver
rg -n "queueInLoop|runInLoop|doPendingFunctors" webserver
rg -n "parseRequest|appendToBuffer|hpSendFile" webserver
rg -n "USE_EPOLL_LT|USE_RINGBUFFER|USE_RECYCLE" webserver
```

这几组搜索足够把核心控制流串起来。

## 11. 如果你要继续改这个项目，先做什么

最推荐的第一步不是加功能，而是先做结构整理：

1. 把配置宏收敛成统一配置入口。
2. 把并发策略、回收策略、IO 模式、buffer 选择抽成策略层。
3. 把 stdout 日志换成统一日志系统。

原因是当前最主要的复杂度已经不在“功能少”，而在“策略差异已经散到主链路里”。

## 12. 读完这份 Wiki 后，下一步看什么

- 想系统理解架构：看 [architecture-guide.md](./architecture-guide.md)
- 想继续做重构规划：看 [change-notes.md](./change-notes.md)

如果你是维护者，可以把这三份文档一起看；  
如果你是第一次接手项目的人，只看这份 Wiki 再配合 IDE 跳转，通常就已经能进入状态了。
