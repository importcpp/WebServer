# WebServer Architecture Guide

## 1. 先用一句话理解这个项目

这是一个基于 Reactor 模式的多线程 C++ WebServer：

- 主线程负责监听和接受连接。
- I/O 线程负责每个连接的读写事件。
- 应用层目前实现了一个简单 HTTP 服务器。
- 编译期开关决定 ET/LT、线性 Buffer / 环形 Buffer、队列/锁策略、连接复用策略等行为。

如果把这个项目压缩成一张图，可以先看下面这张总览图。

```text
runHttpServer.cpp
  |
  +-- EventLoop                  主线程事件循环
  |
  +-- HttpServer                 HTTP 语义层
        |
        +-- TcpServer            TCP 总控
              |
              +-- Acceptor       监听与 accept
              +-- EventLoopThreadPool
              +-- TcpConnection  单连接 I/O 核心
                      |
                      +-- Buffer / RingBuffer
                      +-- HttpContext
                              |
                              +-- HttpRequest
                              +-- HttpResponse

EventLoop
  |
  +-- EventManager(epoll)
        |
        +-- Channel 回调分发
              |
              +-- Acceptor::handleRead()
              +-- TcpConnection::handleRead()/handleWrite()
```

## 2. 目录结构怎么读

`webserver/` 目录可以按职责分成 7 层：

| 目录 | 作用 | 关键文件 |
| --- | --- | --- |
| `loop/` | 事件循环、线程唤醒、loop 线程封装 | `KEventLoop.*`、`KAsyncWaker.*`、`KEventLoopThread.*`、`KEventLoopThreadPool.*` |
| `poller/` | epoll 封装与 Channel 分发 | `KEventManager.*`、`KChannel.*` |
| `tcp/` | 监听、连接、socket、buffer、地址 | `KAcceptor.*`、`KTcpServer.*`、`KTcpConnection.*`、`KBuffer.*`、`KRingBuffer.*` |
| `http/` | HTTP 请求解析与响应组装 | `KHttpContext.*`、`KHttpRequest.h`、`KHttpResponse.*`、`KHttpServer.*` |
| `lock/` | 自旋锁与 lock-free 队列 | `KSpinLock.h`、`KLockFreeQueue.h` |
| `thread/` | 独立线程池实验实现 | `KThreadPool.*` |
| `utils/` | 回调类型、时间戳、基础工具 | `KCallbacks.h`、`KTimestamp.*`、`KTypes.h` |

从阅读顺序上，最推荐：

1. `runHttpServer.cpp`
2. `http/KHttpServer.*`
3. `tcp/KTcpServer.*`
4. `tcp/KTcpConnection.*`
5. `loop/KEventLoop.*`
6. `poller/KEventManager.*` 和 `poller/KChannel.*`
7. `http/KHttpContext.*`
8. `tcp/KBuffer.*` / `tcp/KRingBuffer.*`

## 3. 启动阶段：程序是怎么跑起来的

入口文件是 `webserver/runHttpServer.cpp`。

它做的事情很少：

1. 创建 `EventLoop loop`，作为主线程事件循环。
2. 创建 `HttpServer server(&loop, InetAddress(8888), "httpserver")`。
3. 设置线程数。
4. 调用 `server.start()`。
5. 调用 `loop.loop()` 进入事件循环。

这一步体现了项目的第一层设计哲学：  
**main 几乎不放业务逻辑，只负责把各层对象拼起来。**

启动时序如下：

```text
runHttpServer.cpp
  -> EventLoop                构造主事件循环
  -> HttpServer               构造 HTTP 服务器
      -> TcpServer            构造 TCP 总控
          -> Acceptor         创建监听 socket + Channel
  -> HttpServer::start()
      -> TcpServer::start()
          -> EventLoopThreadPool::start()
          -> EventLoop::runInLoop(Acceptor::listen)
  -> EventLoop::loop()        进入主循环
```

## 4. Reactor 核心：EventLoop、EventManager、Channel

这三个类共同构成“事件通知 -> 事件分发 -> 回调执行”的骨架。

### 4.1 EventLoop：每个线程一个事件循环

文件：

- `webserver/loop/KEventLoop.h`
- `webserver/loop/KEventLoop.cpp`

职责：

- 持有当前线程专属的 `EventManager`
- 驱动 `poll()`
- 保存活跃 `Channel`
- 执行跨线程投递的 functor
- 在必要时通过 `AsyncWaker` 唤醒阻塞中的 loop

主循环长这样：

```text
while (!quit_) {
  activeChannels_.clear();
  pollReturnTime_ = eventmanager_->poll(...);
  for each active channel:
    channel->handleEvent(...)
  doPendingFunctors();
}
```

这是整个服务器最重要的一段控制流。

### 4.2 EventManager：epoll 的直接封装

文件：

- `webserver/poller/KEventManager.h`
- `webserver/poller/KEventManager.cpp`

职责：

- 调用 `epoll_wait`
- 维护 `fd -> Channel*` 映射
- 负责 `EPOLL_CTL_ADD / MOD / DEL`
- 把内核事件填充回 `activeChannels`

它不拥有 `Channel`，只保存裸指针索引。  
这意味着对象所有权仍然在上层，比如 `TcpConnection` 或 `Acceptor`。

### 4.3 Channel：fd 与回调之间的桥

文件：

- `webserver/poller/KChannel.h`
- `webserver/poller/KChannel.cpp`

职责：

- 绑定一个 fd
- 记录自己关心的事件掩码
- 保存 4 类回调：
  - read
  - write
  - error
  - close
- 在 `handleEvent()` 中根据 `revents_` 分发

`Channel` 本身不拥有 fd，这一点非常关键：

- fd 的生命周期由 `Socket` 或其他上层对象管理
- `Channel` 只是“这个 fd 在事件循环中的代表”

## 5. 线程模型：为什么是主线程 accept，I/O 线程处理连接

线程模型由下面两个类承载：

- `webserver/loop/KEventLoopThread.*`
- `webserver/loop/KEventLoopThreadPool.*`

### 5.1 EventLoopThread

`EventLoopThread::startLoop()` 会启动一个新线程，在那个线程里局部创建 `EventLoop loop`，然后通过条件变量把 `loop_` 指针返回给外部。

这是一个典型模式：

- `EventLoop` 必须在所属线程里创建
- 但外部又需要拿到它的地址进行调度

### 5.2 EventLoopThreadPool

`EventLoopThreadPool` 的职责非常单纯：

- 按需创建多个 `EventLoopThread`
- 保存所有子 `EventLoop*`
- 通过 round-robin 分发连接

也就是说，本项目并不是“线程池里跑任务”，而是“线程池里每个线程跑一个事件循环”。

这一点和普通的计算型线程池完全不同。

## 6. 网络层：从监听到连接对象

### 6.1 InetAddress、Socket、KSocketsOps

这三者负责最底层的 socket 细节。

#### InetAddress

文件：

- `webserver/tcp/KInetAddress.h`
- `webserver/tcp/KInetAddress.cpp`

作用：

- 封装 `sockaddr_in`
- 提供端口构造、IP:Port 字符串转换

#### Socket

文件：

- `webserver/tcp/KSocket.h`
- `webserver/tcp/KSocket.cpp`

作用：

- 封装 fd
- 在析构时关闭 fd
- 提供 `bind/listen/accept/shutdownWrite/setTcpNoDelay/setKeepAlive`

#### KSocketsOps

文件：

- `webserver/tcp/KSocketsOps.h`
- `webserver/tcp/KSocketsOps.cpp`

作用：

- 提供纯函数式 socket 操作
- 屏蔽 `sockaddr` 类型转换
- 统一 `accept4`、`bind`、`listen`、`getsockname` 等调用

设计上可以把它理解为“无状态系统调用工具箱”。

### 6.2 Acceptor：监听 socket 的包装

文件：

- `webserver/tcp/KAcceptor.h`
- `webserver/tcp/KAcceptor.cpp`

职责：

- 创建监听 socket
- 创建监听 fd 对应的 `Channel`
- 在 `listen()` 时把可读事件注册到 loop
- 在 `handleRead()` 中 `accept` 新连接
- 通过 `NewConnectionCallback` 把新连接继续上抛

注意这里的重点：

- `Acceptor` 只负责“把连接接进来”
- 它不管理连接对象的生命周期

### 6.3 TcpServer：连接总控

文件：

- `webserver/tcp/KTcpServer.h`
- `webserver/tcp/KTcpServer.cpp`

职责：

- 拥有 `Acceptor`
- 拥有 `EventLoopThreadPool`
- 维护 `connections_` 字典
- 收到新连接后构造 `TcpConnection`
- 设置连接层回调
- 在连接关闭时把它从 map 中移除

`TcpServer` 是 TCP 层真正的“协调者”。

它不直接处理请求数据，但负责把连接放到正确的 I/O 线程里，并维持连接对象生命周期。

新连接分发链路如下：

```text
Base EventLoop
  -> Acceptor::handleRead()
      -> accept()
      -> TcpServer::newConnection(sockfd, peerAddr)
          -> EventLoopThreadPool::getNextLoop()
          -> 选择一个 ioLoop
          -> 构造 TcpConnection
          -> 设置 connection / message / close 回调
          -> ioLoop->runInLoop(TcpConnection::connectEstablished)
```

## 7. 连接层：TcpConnection 才是真正处理 I/O 的地方

文件：

- `webserver/tcp/KTcpConnection.h`
- `webserver/tcp/KTcpConnection.cpp`

这是项目里最核心的业务对象之一。

### 7.1 为什么它用 `shared_ptr`

`TcpConnection` 是少数显式继承 `enable_shared_from_this` 的类。  
原因是：

- 连接对象同时被 `TcpServer::connections_` 管理
- 也会被回调、延迟任务、关闭流程临时持有
- 关闭与销毁并不是一个同步点

如果不用引用计数，很容易在异步关闭过程中悬空。

### 7.2 连接状态机

源码里的状态：

- `kConnecting`
- `kConnected`
- `kDisconnecting`
- `kDisconnected`

典型流转：

```text
kConnecting
  -> connectEstablished()
  -> kConnected
  -> shutdown()
  -> kDisconnecting
  -> handleClose()/connectDestroyed()
  -> kDisconnected
```

### 7.3 读路径

读事件发生时，链路是：

1. `Channel::handleEvent()`
2. `TcpConnection::handleRead()`
3. `inputBuffer_.readFd()` 或 `readFdET()`
4. 调用上层 `messageCallback_`

对 HTTP 来说，这个 `messageCallback_` 最终就是 `HttpServer::onMessage()`。

### 7.4 写路径

写路径分两种：

#### 直接发送

`sendInLoop()` 会先尝试直接 `write()`：

- 如果一次写完，就不需要关注可写事件
- 如果没写完，就把剩余数据放进 `outputBuffer_`

#### 延迟发送

当 `outputBuffer_` 有积压数据时：

- 打开 `channel_->enableWriting()`
- 等待下次可写事件
- 在 `handleWrite()` 中继续 flush

这种设计避免了数据乱序，也避免了在不可写时忙等。

### 7.5 文件发送

`hpSendFile()` 使用 `sendfile()` 发送文件。

在当前实现里，`/file` 路径会：

1. 先发送 HTTP 头
2. 再调用 `sendfile`

这是一个比较直接的 zero-copy 实现路径。

## 8. Buffer 层：为什么有两个实现

### 8.1 KBuffer：线性 Buffer

文件：

- `webserver/tcp/KBuffer.h`
- `webserver/tcp/KBuffer.cpp`

特点：

- 底层是 `std::vector<char>`
- 可读区是连续内存
- 必要时会做挪动或扩容

优点：

- 实现简单
- 解析逻辑自然

缺点：

- 长连接高并发下可能有更多数据搬移

### 8.2 KRingBuffer：环形 Buffer

文件：

- `webserver/tcp/KRingBuffer.h`
- `webserver/tcp/KRingBuffer.cpp`

特点：

- 底层是手工管理的循环数组
- 读写空间可能分裂成两段
- 借助 `readv/writev` 处理不连续内存

优点：

- 减少数据搬移

缺点：

- 上层解析逻辑会复杂不少
- 当前源码里因此出现了一些专门兼容 ringbuffer 的分支

## 9. HTTP 层：从字节流到响应报文

### 9.1 HttpContext：请求解析状态机

文件：

- `webserver/http/KHttpContext.h`
- `webserver/http/KHttpContext.cpp`

状态：

- `kExpectRequestLine`
- `kExpectHeaders`
- `kExpectBody`
- `kGotAll`

职责：

- 从 `Buffer` 中读取一行一行的数据
- 解析请求行
- 解析 Header
- 在完成后把结果写入 `HttpRequest`

这类设计的好处是：  
`HttpContext` 不关心 socket 和线程，只关心“给我一个可读缓存，我把它解析成请求对象”。

### 9.2 HttpRequest

文件：

- `webserver/http/KHttpRequest.h`

职责：

- 保存方法、版本、路径、查询串、接收时间、请求头

这是一个非常轻量的值对象。

### 9.3 HttpResponse

文件：

- `webserver/http/KHttpResponse.h`
- `webserver/http/KHttpResponse.cpp`

职责：

- 保存状态码、头、body、是否关闭连接
- 把响应序列化进 `Buffer`

### 9.4 HttpServer

文件：

- `webserver/http/KHttpServer.h`
- `webserver/http/KHttpServer.cpp`

职责：

- 在 TCP 层之上装配 HTTP 语义
- 连接建立时创建 `HttpContext`
- 收到字节流时交给 `HttpContext` 解析
- 根据 `HttpRequest` 生成 `HttpResponse`

当前支持的典型路径：

- `/hello`
- `/good`
- `/`
- `/favicon.ico`
- `/file`

`/file` 是当前实现里的一个特殊路径：  
它会显式读取 `./index.html`，先拼 HTTP 头，再调用 `sendfile()` 发送文件内容。

## 10. 跨线程任务：AsyncWaker 为什么重要

文件：

- `webserver/loop/KAsyncWaker.h`
- `webserver/loop/KAsyncWaker.cpp`

`EventLoop` 的一个关键问题是：

- `epoll_wait()` 正在阻塞
- 另一个线程想投递一个 functor
- 如果不唤醒 loop，回调可能要等一个 poll 周期

这里的做法是：

1. 用 `eventfd` 创建一个专门的唤醒 fd
2. 给这个 fd 绑定一个 `Channel`
3. 其他线程 `write(eventfd)`
4. loop 线程读掉它，并继续处理 `pendingFunctors_`

这就是 `queueInLoop()` 能在多线程下工作的关键。

## 11. 回调设计：为什么 KCallbacks.h 很关键

文件：

- `webserver/utils/KCallbacks.h`

这里定义了：

- `ConnectionCallback`
- `MessageCallback`
- `WriteCompleteCallback`
- `CloseCallback`
- `RecycleCallback`

这套回调类型让：

- `TcpServer`
- `TcpConnection`
- `HttpServer`

之间形成了很清晰的层次关系：

- TCP 层负责 I/O 与生命周期
- HTTP 层只通过回调接入

## 12. 编译期开关对架构的影响

这是当前项目非常重要的一部分。

| 开关 | 当前意义 | 对架构的影响 |
| --- | --- | --- |
| `USE_EPOLL_LT` | LT/ET 选择 | 改变 accept/read/write 处理方式 |
| `USE_RINGBUFFER` | 线性/环形 buffer 选择 | 改变 buffer 接口语义与解析路径 |
| `USE_LOCKFREEQUEUE` | functor 队列实现 | 改变 `EventLoop` 内部并发模型 |
| `USE_SPINLOCK` | 锁选择 | 改变 `EventLoop` / `TcpServer` 的临界区实现 |
| `USE_RECYCLE` | 连接复用 | 改变 `TcpConnection` 生命周期 |
| `USE_STD_COUT` | 日志打印 | 影响运行期开销与依赖关系 |

这些开关让项目具备实验性和对比性，但也提高了维护复杂度。

## 13. 当前设计的优点

### 优点 1：主链路很清晰

从 `main -> HttpServer -> TcpServer -> TcpConnection -> HttpContext` 这条链路非常清楚，适合教学和演示。

### 优点 2：对象职责边界基本合理

- `Acceptor` 只 accept
- `TcpServer` 只管理连接
- `TcpConnection` 只做单连接 I/O
- `HttpContext` 只解析请求

### 优点 3：性能导向明显

源码里能明显看到作者在做这些优化尝试：

- ET 模式
- ringbuffer
- lock-free queue
- `sendfile`
- 连接复用

## 14. 当前设计的局限

### 局限 1：宏分支已经渗入业务层

这会导致：

- 代码阅读成本高
- 单元测试组合复杂
- 改一个策略容易波及主链路

### 局限 2：部分对象所有权还不够现代化

例如：

- `AsyncWaker` 里使用裸指针保存 `Channel`
- 老式 `std::bind` 与裸指针结合较多

### 局限 3：日志系统仍是 stdout 风格

这对高并发服务不够友好。

### 局限 4：HTTP 层与文件发送逻辑耦合较深

当前 `/file` 路径直接在 `HttpServer::onRequest()` 中处理文件打开、文件长度、发送头和文件体，扩展性一般。

## 15. 如果你要继续扩展这个项目，最推荐的方向

### 方向 1：先做结构整理，再做功能扩展

优先整理：

- 配置入口
- 策略层
- 日志层
- buffer 统一接口

### 方向 2：把 HTTP 层继续抽薄

可考虑把：

- 静态文件处理
- 路由分发
- 响应序列化

进一步独立成单独模块。

### 方向 3：补测试与压测脚本

当前项目更像“可运行的高性能网络实验平台”，而不是“工业级带完整回归体系的框架”。  
继续走下去，测试和 benchmark 维度必须补齐。

## 16. 一句话总结

这个项目最值得学习的地方，不是它实现了一个简单 HTTP server，而是它把 **Reactor、事件循环、连接管理、跨线程唤醒、编译期策略切换** 这些核心思想都非常集中地放在了一起。

如果你想从源码层理解一个 C++ 网络服务器是怎么跑起来的，这个项目非常适合阅读；  
如果你想把它继续演进成更稳定、更可维护的框架，那么下一步的关键工作就是：**把策略层从业务层剥离出来**。
