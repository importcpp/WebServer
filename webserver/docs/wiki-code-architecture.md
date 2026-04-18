# WebServer Wiki: Code Architecture Walkthrough

## 1. 这份 Wiki 怎么用

这份文档不是泛泛地讲“服务器架构是什么”，而是回答一个更实际的问题：

> 如果我现在打开 IDE，要从哪里开始看这套代码，才能最快进入状态？

适合的场景：

- 你第一次接手这个项目
- 你想在 30 分钟到 1 小时内建立代码地图
- 你准备 debug 一条请求链路
- 你准备在当前 `optimize` 工作树上继续做重构

## 2. 先记住这张源码地图

```text
runHttpServer.cpp
  -> HttpServer
      -> TcpServer
          -> Acceptor
          -> EventLoopThreadPool
          -> TcpConnection
              -> Buffer / RingBuffer
              -> Channel
                  -> EventLoop
                      -> EventManager(epoll)
      -> HttpContext
          -> HttpRequest
      -> HttpResponse
      -> StaticFileCache

策略层:
  KBuildConfig
    -> KPendingFunctorQueue
    -> KTcpIoMode
    -> KSelectedBuffer
    -> KTcpConnectionRecycler
    -> KTcpConnectionLifecycle
    -> KAsyncLogger
```

如果你只能记住三句话：

1. `EventLoop` 是线程内调度中心。
2. `TcpConnection` 是单连接 I/O 核心。
3. `HttpContext` 是 HTTP 请求解析核心。

## 3. 一小时阅读路线

### 第 1 站：看程序入口

文件：

- [webserver/runHttpServer.cpp](../runHttpServer.cpp)

你要带着这几个问题看：

- 主线程到底创建了哪些对象？
- 谁先启动？
- 为什么最后是 `loop.loop()` 而不是 `server.loop()`？

看完后你应该得到一个结论：

- 这是一套以 `EventLoop` 为中心驱动的系统
- `HttpServer` 只是挂在 `EventLoop` 上的一层业务壳

### 第 2 站：看 HTTP 层是怎么接进来的

文件：

- [webserver/http/KHttpServer.h](../http/KHttpServer.h)
- [webserver/http/KHttpServer.cpp](../http/KHttpServer.cpp)

重点看：

- `HttpServer::HttpServer()`
- `HttpServer::start()`
- `HttpServer::onConnection()`
- `HttpServer::onMessage()`
- `HttpServer::onRequest()`

你要回答的问题：

- HTTP 层是怎么把回调挂到 `TcpServer` 上的？
- `HttpContext` 是在哪里创建的？
- `/file` 路径为什么和普通路径不同？

### 第 3 站：看 TCP 总控

文件：

- [webserver/tcp/KTcpServer.h](../tcp/KTcpServer.h)
- [webserver/tcp/KTcpServer.cpp](../tcp/KTcpServer.cpp)

重点看：

- `TcpServer::start()`
- `TcpServer::newConnection()`
- `TcpServer::configureConnection()`
- `TcpServer::removeConnectionInLoop()`

你要回答的问题：

- 新连接为什么不在 accept 线程直接处理？
- 为什么连接对象要放进 `connections_` map？
- 当前分支里的 recycler 是怎么工作的？

### 第 4 站：看单连接 I/O 核心

文件：

- [webserver/tcp/KTcpConnection.h](../tcp/KTcpConnection.h)
- [webserver/tcp/KTcpConnection.cpp](../tcp/KTcpConnection.cpp)

重点看：

- `connectEstablished()`
- `handleRead()`
- `sendInLoop()`
- `handleWrite()`
- `handleClose()`
- `connectDestroyed()`
- `hpSendFile()`

你要回答的问题：

- 一个连接对象从创建到销毁经历了哪些状态？
- 为什么写数据时不是总先进 `outputBuffer_`？
- `sendfile` 在当前分支里是如何融入普通发送流程的？

### 第 5 站：看 Reactor 核心

文件：

- [webserver/loop/KEventLoop.h](../loop/KEventLoop.h)
- [webserver/loop/KEventLoop.cpp](../loop/KEventLoop.cpp)
- [webserver/poller/KEventManager.h](../poller/KEventManager.h)
- [webserver/poller/KEventManager.cpp](../poller/KEventManager.cpp)
- [webserver/poller/KChannel.h](../poller/KChannel.h)
- [webserver/poller/KChannel.cpp](../poller/KChannel.cpp)

你要回答的问题：

- `EventLoop` 和 `EventManager` 的边界是什么？
- `Channel` 为什么不拥有 fd？
- 为什么跨线程提交任务不能直接操作连接对象？

### 第 6 站：看策略层抽象

文件：

- [webserver/utils/KBuildConfig.h](../utils/KBuildConfig.h)
- [webserver/loop/KPendingFunctorQueue.h](../loop/KPendingFunctorQueue.h)
- [webserver/tcp/KTcpIoMode.h](../tcp/KTcpIoMode.h)
- [webserver/tcp/KSelectedBuffer.h](../tcp/KSelectedBuffer.h)
- [webserver/tcp/KTcpConnectionRecycler.h](../tcp/KTcpConnectionRecycler.h)
- [webserver/tcp/KTcpConnectionLifecycle.h](../tcp/KTcpConnectionLifecycle.h)

这是当前分支最值得重点看的地方，因为它们体现了结构重构的方向。

你要回答的问题：

- 为什么当前分支增加了这些文件？
- 它们把哪些宏分支从业务层里拿走了？
- 如果以后继续加策略，是不是还可以沿着同样的模式扩展？

### 第 7 站：最后看 Buffer 和 HTTP 解析

文件：

- [webserver/tcp/KBuffer.h](../tcp/KBuffer.h)
- [webserver/tcp/KBuffer.cpp](../tcp/KBuffer.cpp)
- [webserver/tcp/KRingBuffer.h](../tcp/KRingBuffer.h)
- [webserver/tcp/KRingBuffer.cpp](../tcp/KRingBuffer.cpp)
- [webserver/http/KHttpContext.h](../http/KHttpContext.h)
- [webserver/http/KHttpContext.cpp](../http/KHttpContext.cpp)

你要回答的问题：

- 两种 Buffer 的能力差异是什么？
- 当前分支是怎么统一 `HttpContext` 解析逻辑的？
- ringbuffer 模式下为什么只有跨 span 时才做局部拷贝？

## 4. 如果你只想追一条请求链路

最推荐按下面这个顺序追踪：

```text
runHttpServer.cpp
  -> HttpServer::start()
  -> TcpServer::start()
  -> Acceptor::listen()
  -> Acceptor::handleRead()
  -> TcpServer::newConnection()
  -> TcpConnection::connectEstablished()
  -> TcpConnection::handleRead()
  -> HttpServer::onMessage()
  -> HttpContext::parseRequest()
  -> HttpServer::onRequest()
  -> HttpResponse::appendToBuffer()
  -> TcpConnection::send()/sendInLoop()
  -> TcpConnection::handleWrite()
```

如果是静态文件路径 `/file`，再补上：

```text
HttpServer::onRequest()
  -> StaticFileCache::find()
  -> HttpServer::sendResponse()
  -> TcpConnection::send()
  -> TcpConnection::hpSendFile()
  -> TcpConnection::sendFileInLoop()
```

## 5. 如果你想搞懂“跨线程任务为什么安全”

重点看：

- [webserver/loop/KEventLoop.cpp](../loop/KEventLoop.cpp)
- [webserver/loop/KAsyncWaker.cpp](../loop/KAsyncWaker.cpp)
- [webserver/loop/KEventLoopThread.cpp](../loop/KEventLoopThread.cpp)
- [webserver/loop/KEventLoopThreadPool.cpp](../loop/KEventLoopThreadPool.cpp)

理解方式：

1. `EventLoop` 只能在所属线程里安全操作。
2. 其他线程如果要让它做事，必须走 `queueInLoop()`。
3. `queueInLoop()` 把任务塞进 `pendingFunctors_`。
4. 如果 loop 当前阻塞在 `epoll_wait`，就由 `AsyncWaker` 用 `eventfd` 唤醒它。
5. loop 醒来后在 `doPendingFunctors()` 中执行这些任务。

所以这套线程安全不是靠“到处加锁”，而是靠：

- loop 线程亲和性
- 少量跨线程任务投递
- eventfd 唤醒

## 6. 如果你想看当前分支和原始版本的差异

那就不要只盯主链路，优先看这些文件：

- [webserver/utils/KBuildConfig.h](../utils/KBuildConfig.h)
- [webserver/loop/KPendingFunctorQueue.h](../loop/KPendingFunctorQueue.h)
- [webserver/tcp/KTcpIoMode.h](../tcp/KTcpIoMode.h)
- [webserver/tcp/KSelectedBuffer.h](../tcp/KSelectedBuffer.h)
- [webserver/tcp/KTcpConnectionRecycler.h](../tcp/KTcpConnectionRecycler.h)
- [webserver/tcp/KTcpConnectionLifecycle.h](../tcp/KTcpConnectionLifecycle.h)
- [webserver/utils/KAsyncLogger.h](../utils/KAsyncLogger.h)
- [webserver/http/KStaticFileCache.h](../http/KStaticFileCache.h)

这些文件就是当前分支最核心的“结构优化痕迹”。

## 7. 类速查表

| 类 / 组件 | 文件 | 作用 | 最先看的函数 |
| --- | --- | --- | --- |
| `EventLoop` | `loop/KEventLoop.*` | 线程内事件循环与 functor 调度中心 | `loop()`、`queueInLoop()` |
| `AsyncWaker` | `loop/KAsyncWaker.*` | 用 `eventfd` 唤醒阻塞中的 loop | `wakeup()` |
| `EventManager` | `poller/KEventManager.*` | epoll 封装与 fd->Channel 管理 | `poll()`、`updateChannel()` |
| `Channel` | `poller/KChannel.*` | fd 与回调的桥 | `handleEvent()` |
| `Acceptor` | `tcp/KAcceptor.*` | 监听 socket 与 accept 分发 | `handleRead()` |
| `TcpServer` | `tcp/KTcpServer.*` | 新连接分配、连接 map 管理 | `newConnection()` |
| `TcpConnection` | `tcp/KTcpConnection.*` | 单连接收发、状态机、发送缓冲 | `handleRead()`、`sendInLoop()` |
| `HttpServer` | `http/KHttpServer.*` | 在 TCP 层上装配 HTTP 语义 | `onMessage()`、`onRequest()` |
| `HttpContext` | `http/KHttpContext.*` | HTTP 请求解析状态机 | `parseRequest()` |
| `StaticFileCache` | `http/KStaticFileCache.*` | 静态文件缓存 | `find()` |
| `KPendingFunctorQueue` | `loop/KPendingFunctorQueue.h` | EventLoop 内部任务队列策略层 | `push()`、`consumeAll()` |
| `KTcpIoMode` | `tcp/KTcpIoMode.h` | LT/ET IO 模式抽象 | `read()`、`write()` |
| `KSelectedBuffer` | `tcp/KSelectedBuffer.h` | Buffer 类型选择入口 | 选头本身 |

## 8. 调试时推荐打断点的位置

如果你想最快看懂运行行为，推荐这些断点：

- `Acceptor::handleRead`
- `TcpServer::newConnection`
- `TcpConnection::connectEstablished`
- `TcpConnection::handleRead`
- `HttpServer::onMessage`
- `HttpContext::parseRequest`
- `HttpServer::onRequest`
- `TcpConnection::sendInLoop`
- `TcpConnection::handleWrite`
- `TcpConnection::connectDestroyed`

如果你只打一个断点，建议先打在 `TcpConnection::handleRead()`。

这是最容易把“网络层 -> HTTP 层 -> 回包”串起来的位置。

## 9. 推荐的源码搜索命令

可以直接在仓库根目录执行：

```bash
rg -n "newConnection|handleRead|handleWrite|connectDestroyed" webserver
rg -n "queueInLoop|runInLoop|doPendingFunctors" webserver
rg -n "parseRequest|appendToBuffer|sendFileInLoop|hpSendFile" webserver
rg -n "KBuildConfig|KPendingFunctorQueue|KTcpIoMode|KSelectedBuffer" webserver
```

这几组搜索基本就能把当前分支的主链路和重构线索一起串起来。

## 10. 常见误区

### 误区 1：以为 `HttpServer` 是主角

不是。  
真正驱动所有请求的是：

- `EventLoop`
- `EventManager`
- `Channel`
- `TcpConnection`

`HttpServer` 只是应用层壳。

### 误区 2：以为 `KThreadPool` 是这套服务器真正的 I/O 线程池

不是。  
真正服务网络 I/O 的是：

- `EventLoopThread`
- `EventLoopThreadPool`

`webserver/thread/KThreadPool.*` 更偏实验实现。

### 误区 3：以为当前分支只是“修了几个宏”

也不是。  
当前分支的核心意义在于：

- 它开始把宏差异系统性地收敛成策略层

这决定了后续这套代码还能不能继续演进。

## 11. 如果你准备继续改这套代码，先改什么

推荐顺序：

1. 继续收敛底层 `Buffer` 类型定义
2. 给 `HttpContext` 拆更细的解析函数
3. 补测试矩阵，覆盖 LT/ET、RingBuffer/LinearBuffer、recycle on/off、并发策略组合
4. 继续整理文档与 README 的边界

如果你只做功能扩展，不先稳住这几层，后面理解成本会越来越高。

## 12. 一句话总结

这套代码现在最值得看的，不只是“Reactor 怎么写”，而是：

> 它如何在保留原始 WebServer 主链路的前提下，把配置差异逐步收敛成更清晰的配置层和策略层。

从学习角度，这非常适合作为“读懂一个 C++ 网络服务 + 看懂一轮架构重构”的双重样本。
