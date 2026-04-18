# WebServer Change Notes

## 1. 文档目的

这份文档聚焦“当前分支相对原始实现到底改了什么”，尤其是那些会影响理解成本、维护方式和扩展路径的结构性改动。

它不是 changelog，也不是提交记录复述。它重点回答三个问题：

1. 这套代码现在和早期版本相比，架构上变了什么。
2. 每个改动解决了什么问题。
3. 还有哪些可以继续改进，但目前还没有完全收口。

## 2. 当前代码基线

当前工作树保留了原始 WebServer 的核心设计：

- `EventLoop + epoll + Channel` 的 Reactor 主干
- `TcpServer + TcpConnection` 的 TCP 连接管理
- `HttpServer + HttpContext + HttpRequest + HttpResponse` 的 HTTP 层
- 编译期开关控制 LT/ET、Buffer 类型、并发队列、连接回收、日志行为

但在这个基础上，当前分支已经新增了一批“策略层 / 配置层”抽象，用来减少业务层对宏分支的直接感知。

## 3. 当前分支已经落地的关键改动

### 3.1 统一配置入口：`KBuildConfig`

文件：

- [webserver/utils/KBuildConfig.h](../utils/KBuildConfig.h)

作用：

- 把 `USE_RECYCLE`、`USE_EPOLL_LT`、`USE_RINGBUFFER` 从“业务层直接感知的宏”收敛为 `constexpr bool` 配置常量。

现在暴露的统一配置包括：

- `kEnableConnectionRecycle`
- `kUseEpollLT`
- `kUseRingBuffer`

解决的问题：

- 业务代码不需要反复写 `#ifdef USE_XXX`
- 后续策略层可以用 `if constexpr` 或 `std::conditional_t`
- 搜索宏时，能很快区分“配置入口”和“业务实现”

### 3.2 EventLoop 的 pending functor 队列抽象

文件：

- [webserver/loop/KPendingFunctorQueue.h](../loop/KPendingFunctorQueue.h)
- [webserver/loop/KEventLoop.h](../loop/KEventLoop.h)
- [webserver/loop/KEventLoop.cpp](../loop/KEventLoop.cpp)

改动前：

- `EventLoop` 直接在类内部分叉：
  - `LockFreeQueue`
  - `std::vector + std::mutex`
  - `std::vector + SpinLock`

改动后：

- `PendingFunctorQueue<T>` 成为统一策略别名
- `EventLoop` 只调用：
  - `pendingFunctors_.push(...)`
  - `pendingFunctors_.consumeAll(...)`

解决的问题：

- `EventLoop` 不再同时承担“事件循环”和“并发容器选择”两种职责
- `USE_LOCKFREEQUEUE` / `USE_SPINLOCK` 不再散在业务流程里

这一步的价值很高，因为 `EventLoop` 是项目的中心对象，任何宏分支都很容易扩散理解成本。

### 3.3 LT/ET IO 模式抽象：`KTcpIoMode`

文件：

- [webserver/tcp/KTcpIoMode.h](../tcp/KTcpIoMode.h)
- [webserver/tcp/KAcceptor.cpp](../tcp/KAcceptor.cpp)
- [webserver/tcp/KTcpConnection.cpp](../tcp/KTcpConnection.cpp)
- [webserver/poller/KChannel.h](../poller/KChannel.h)

改动前：

- `USE_EPOLL_LT` 分散在：
  - accept 循环
  - `TcpConnection::handleRead`
  - `TcpConnection::handleWrite`
  - `sendInLoop`
  - `Channel::enableEpollET`

改动后：

- 统一通过 `tcp_io_mode` helper 暴露：
  - `acceptOneConnectionPerEvent()`
  - `read(buffer, fd, savedErrno)`
  - `write(buffer, fd, savedErrno)`
  - `writeDirect(fd, payload)`

解决的问题：

- LT/ET 差异从业务方法内部退到 helper
- `TcpConnection` 的主流程更接近“纯连接逻辑”
- `Acceptor` 的 accept 行为更容易读懂

### 3.4 Buffer 选择抽象：`KSelectedBuffer`

文件：

- [webserver/tcp/KSelectedBuffer.h](../tcp/KSelectedBuffer.h)
- [webserver/tcp/KBuffer.h](../tcp/KBuffer.h)
- [webserver/tcp/KRingBuffer.h](../tcp/KRingBuffer.h)
- [webserver/http/KHttpContext.cpp](../http/KHttpContext.cpp)
- [webserver/http/KHttpResponse.cpp](../http/KHttpResponse.cpp)
- [webserver/tcp/KTcpConnection.h](../tcp/KTcpConnection.h)

改动前：

- 上层代码经常要自己 `#ifdef USE_RINGBUFFER`
- `HttpContext` 维护了两套请求解析逻辑
- `TcpConnection::send(Buffer*)` 维护了两套路径

改动后：

- 统一通过 `KSelectedBuffer.h` 选中当前 `Buffer`
- 给两种 Buffer 补齐统一能力接口，例如：
  - `isReadableContiguous()`
  - `isSpanContiguous()`
  - `readableView()`
  - `readableStringUntil()`
  - `retrieveLineAndCRLF()`

带来的直接收益：

- `HttpContext::parseRequest()` 收敛成一套统一状态机逻辑
- `HttpResponse` 不再感知线性/环形 Buffer 差异
- `TcpConnection::send(Buffer*)` 更自然地走“直接 view / 回退 string”两层路径

### 3.5 连接复用拆分成 recycler + lifecycle

文件：

- [webserver/tcp/KRecycledConnectionPool.h](../tcp/KRecycledConnectionPool.h)
- [webserver/tcp/KTcpConnectionRecycler.h](../tcp/KTcpConnectionRecycler.h)
- [webserver/tcp/KTcpConnectionLifecycle.h](../tcp/KTcpConnectionLifecycle.h)
- [webserver/tcp/KTcpServer.h](../tcp/KTcpServer.h)
- [webserver/tcp/KTcpServer.cpp](../tcp/KTcpServer.cpp)
- [webserver/tcp/KTcpConnection.h](../tcp/KTcpConnection.h)
- [webserver/tcp/KTcpConnection.cpp](../tcp/KTcpConnection.cpp)

改动前：

- `USE_RECYCLE` 逻辑散在 `TcpServer` 和 `TcpConnection` 两边
- `SpinLock` 裸露在 `KTcpServer.h`
- 连接池管理、回收节流、资源释放顺序混在一起

改动后：

- `TcpConnectionRecycler<T>` 管理“能不能拿回连接、怎么回收连接”
- `TcpConnectionLifecycle` 管理“连接销毁时要不要进入 recycle 生命周期”
- `RecycledConnectionPool<T>` 封装底层容器与锁策略

解决的问题：

- `TcpServer` 只关注：
  - `tryTake(conn)`
  - `configureConnection(conn)`
  - `recycleConnection(conn)`
- `TcpConnection::connectDestroyed()` 只需要把生命周期钩子交给 `recycleLifecycle_`

### 3.6 异步文件日志：`KAsyncLogger`

文件：

- [webserver/utils/KAsyncLogger.h](../utils/KAsyncLogger.h)
- [webserver/utils/KAsyncLogger.cpp](../utils/KAsyncLogger.cpp)
- [CMakeLists.txt](../../CMakeLists.txt)
- [Makefile](../../Makefile)

改动前：

- 大量 `std::cout` / `std::cerr`
- 业务线程直接承担日志输出开销
- 头文件里会引入 `iostream`

改动后：

- 支持 `USE_ASYNC_FILE_LOGGING`
- 通过统一日志宏收口
- 后台线程异步刷盘，支持环境变量指定日志文件路径

解决的问题：

- 降低日志对性能路径的干扰
- 降低 `iostream` 依赖传播
- 日志格式统一

### 3.7 静态文件缓存：`KStaticFileCache`

文件：

- [webserver/http/KStaticFileCache.h](../http/KStaticFileCache.h)
- [webserver/http/KStaticFileCache.cpp](../http/KStaticFileCache.cpp)
- [webserver/http/KHttpServer.h](../http/KHttpServer.h)
- [webserver/http/KHttpServer.cpp](../http/KHttpServer.cpp)

这是当前分支里另一个很重要的增强点：

- 静态文件不再每次请求都重新 `stat/open/close`
- 增加了线程安全缓存
- `HttpServer` 通过 `setStaticFileRoot()` 和 `StaticFileCache::find()` 获取文件

它使 `/file` 路径从“演示级直接读文件”升级成“可缓存、可复用”的实现。

## 4. 从宏驱动到策略驱动：当前这条分支的设计方向

把这些改动放在一起看，会发现它们不是零散 patch，而是在朝同一个方向收敛：

> 把“编译期开关决定的行为差异”从业务对象中抽离出来，收敛成可替换的策略层。

这一点体现在 4 个方面：

1. 配置集中化：`KBuildConfig`
2. 并发策略：`KPendingFunctorQueue`
3. IO 模式策略：`KTcpIoMode`
4. 生命周期策略：`KTcpConnectionRecycler` / `KTcpConnectionLifecycle`

这种方向是对的，因为：

- 主链路类变短了
- 宏数量没有变少，但宏的“影响半径”缩小了
- 后续加新策略时，更容易找到切入点

## 5. 当前代码仍然可以继续改进的地方

### 5.1 `Buffer` / `RingBuffer` 底层仍然保留宏裁剪

虽然上层已经通过 `KSelectedBuffer` 统一了入口，但底层 `KBuffer.h/.cpp` 和 `KRingBuffer.h/.cpp` 仍然是通过预处理器控制“同名类 `Buffer`”。

这意味着：

- 业务层已经轻了
- 但底层实现还是带有编译器级重定义技巧

后续可以进一步演进成：

- `LinearBuffer`
- `RingBuffer`
- `using SelectedBuffer = ...`

这样类型关系会更直观。

### 5.2 `HttpContext` 仍是单体状态机

当前的统一解析已经比以前干净很多，但它仍然把：

- 请求行解析
- Header 解析
- 跨 ring span 处理

都放在同一个函数里。

如果未来继续支持：

- body
- chunked
- pipeline

那就需要进一步拆分小函数。

### 5.3 `EventLoop` 与 `EventManager` 的命名和 ownership 还能再现代化

例如：

- `eventmanager_` 可以改成 `poller_`
- 某些旧式命名仍然偏过程式

这不是功能问题，但会影响长期维护体验。

### 5.4 文档与 README 仍有历史描述残留

当前 `README.md` 仍然混合了：

- 原始实现历史
- 当前分支新增结构

这也是本次补充 `webserver/docs/` 的原因之一。后面如果要继续维护，建议把顶层 README 控制在“安装、构建、运行、特性总览”四块，不要承载过多架构细节。

## 6. 这条分支的价值总结

如果只用一句话概括当前分支的核心价值：

> 它没有推翻原来的 Reactor/TCP/HTTP 主链路，而是在不改变主干模型的前提下，把原本散落在业务层里的编译期差异，逐步收敛成更明确的配置层和策略层。

从维护视角看，这比单纯“继续堆功能”更重要，因为它决定了这套代码后面还能不能继续健康演进。

## 7. 推荐的下一步

如果你准备继续沿着这条线重构，推荐顺序是：

1. 彻底收口 `Buffer` 类型定义，摆脱“同名 `Buffer` + 宏选头”
2. 给 `HttpContext` 增加 body / chunked 支持前，先把解析函数拆小
3. 把顶层 README 和 `webserver/docs/` 联动起来
4. 增加自动化回归矩阵，覆盖：
   - LT / ET
   - LinearBuffer / RingBuffer
   - recycle on / off
   - lockfree / spinlock / mutex

如果这些都做到，这条分支的结构会比最早版本稳定得多，也更适合作为后续继续扩展的基础。
