# WebServer Change Notes

## 1. 文档目的

这份文档不是简单重复 README，而是从“维护者要怎么继续改”这个角度，梳理当前源码的结构现状、痛点位置、影响面以及后续推荐的重构路线。

这里有一个很重要的前提：

- 本文以当前仓库里的实际源码为准。
- 文中提到的“建议改动”“后续重构方向”是基于当前代码分析出来的可落地方案，不应误读为“已经全部完成”。

## 2. 当前代码基线

当前实现是一个典型的 Reactor 风格 WebServer，主干链路如下：

```text
runHttpServer.cpp
  -> EventLoop
  -> HttpServer
  -> TcpServer
  -> Acceptor / EventLoopThreadPool / TcpConnection
  -> HttpContext / HttpRequest / HttpResponse
```

编译期开关对行为影响较大，主要包括：

| 开关 | 作用 | 影响范围 |
| --- | --- | --- |
| `USE_EPOLL_LT` | 决定 LT/ET 模式 | `KAcceptor.cpp`、`KTcpConnection.cpp`、`KBuffer.cpp`、`KRingBuffer.cpp`、`KChannel.h` |
| `USE_RINGBUFFER` | 决定使用线性 Buffer 还是环形 Buffer | `KTcpConnection.h`、`KHttpContext.cpp`、`KHttpResponse.cpp`、`KBuffer/KRingBuffer` |
| `USE_LOCKFREEQUEUE` | 决定 `EventLoop` 的 pending functor 队列实现 | `KEventLoop.h/.cpp` |
| `USE_SPINLOCK` | 决定 `EventLoop` 与 `TcpServer` 的某些临界区锁实现 | `KEventLoop.h/.cpp`、`KTcpServer.h/.cpp` |
| `USE_RECYCLE` | 决定是否复用 `TcpConnection` | `KTcpServer.h/.cpp`、`KTcpConnection.cpp` |
| `USE_STD_COUT` | 控制日志输出 | 多个 `.cpp` 文件 |

这些开关都有效，但也带来了一个明显问题：**宏分支已经渗透到业务层和对象生命周期里**。

## 3. 当前源码的主要维护痛点

### 3.1 编译期开关泄漏到业务层

最明显的例子有：

- `webserver/tcp/KTcpConnection.cpp`
- `webserver/tcp/KAcceptor.cpp`
- `webserver/http/KHttpContext.cpp`
- `webserver/loop/KEventLoop.cpp`
- `webserver/tcp/KTcpServer.h`

问题不在于“用了宏”，而在于：

1. 宏直接改变业务流程。
2. 同一个类同时承担业务职责和策略选择职责。
3. 阅读代码时必须不停在“当前行为”和“其他编译配置下行为”之间切换。

### 3.2 Buffer 选择对上层代码有侵入

当前 `Buffer` 与 `RingBuffer` 通过预处理器让“同名类 `Buffer`”在不同构建下指向不同头文件。这种做法短期有效，但会带来几个问题：

- 上层代码必须知道当前是否启用了 `USE_RINGBUFFER`。
- `HttpContext` 为了兼容 ringbuffer，单独维护了一套请求解析流程。
- `TcpConnection::send()` 在 ringbuffer 路径和线性 buffer 路径上出现不同分支。

影响文件：

- `webserver/tcp/KTcpConnection.h`
- `webserver/http/KHttpContext.cpp`
- `webserver/http/KHttpResponse.cpp`

### 3.3 `EventLoop` 的 pending functor 实现耦合了队列和锁策略

当前 `EventLoop` 同时在处理：

- 回调任务语义
- 锁 free 队列选择
- 自旋锁 / 互斥锁选择

这让 `KEventLoop.h/.cpp` 的宏分支比较重，不利于后续扩展新的并发策略。

### 3.4 `TcpConnection` 回收逻辑分散在两个类里

当前连接复用链路横跨：

- `KTcpServer::newConnection()`
- `KTcpServer::recycleCallback()`
- `TcpConnection::connectDestroyed()`

这意味着一个“是否复用连接”的开关会同时影响：

- 连接池管理
- 生命周期释放顺序
- 回调绑定方式
- 资源清理时机

这类逻辑更适合单独抽成 lifecycle / recycler 策略层。

### 3.5 日志仍是 `std::cout`

源码里大量日志直接使用：

- `std::cout`
- `std::cerr`

问题包括：

- 业务线程直接承担 IO 输出开销
- 头文件中引入 `iostream`
- 日志格式不统一
- 与高并发网络路径耦合过深

### 3.6 代码风格处于“可运行优先”状态

当前工程具备完整主链路，但仍保留一些较早期风格：

- 相对路径 include 混杂
- `std::bind` 使用较多
- 原始指针与 `unique_ptr/shared_ptr` 混用
- 一些对象名较旧，例如 `listenning_`、`eventmanager_`
- `README` 与实际源码已有一定偏差

## 4. 建议的重构路线

下面这组改动按投入收益比排序，适合逐步推进。

### 4.1 第一步：建立统一配置入口

目标：

- 不再让业务层直接 `#ifdef USE_XXX`
- 把编译期开关先收敛到一个地方

建议做法：

- 新增类似 `KBuildConfig.h` 的配置头
- 暴露 `constexpr bool` 常量，例如：
  - `kUseEpollLT`
  - `kUseRingBuffer`
  - `kEnableConnectionRecycle`

收益：

- 业务代码改为普通 C++ 分支或策略选择
- 搜索宏时只会剩下配置入口和底层实现

### 4.2 第二步：把 `EventLoop` 的并发策略抽成单独类

目标：

- 把 `pendingFunctors_` 从 `EventLoop` 本体中拆出来

建议抽象：

```cpp
template <typename Functor>
class PendingFunctorQueue {
public:
  void push(Functor&&);
  template <typename Consumer>
  void consumeAll(Consumer&&);
};
```

可选后端：

- `LockFreePendingFunctorQueue`
- `LockedPendingFunctorQueue<std::mutex>`
- `LockedPendingFunctorQueue<SpinLock>`

收益：

- `KEventLoop.cpp` 的宏分支显著减少
- 新增策略时不需要碰业务流程

### 4.3 第三步：把连接复用拆成 lifecycle + recycler

目标：

- `TcpServer` 只管“拿连接 / 配连接 / 放回连接池”
- `TcpConnection` 只管“销毁时要不要走 recycle 生命周期”

建议拆分：

- `TcpConnectionRecycler`
- `TcpConnectionLifecycle`
- `RecycledConnectionPool`

收益：

- `USE_RECYCLE` 不再散落在 `KTcpServer` 和 `KTcpConnection`
- 生命周期边界更清晰

### 4.4 第四步：把 LT/ET 行为抽成 IO 策略层

目标：

- 业务层不再直接知道自己是 LT 还是 ET

建议抽象：

- `acceptOneConnectionPerEvent()`
- `read(buffer, fd, savedErrno)`
- `write(buffer, fd, savedErrno)`
- `writeDirect(fd, payload)`

收益：

- `KAcceptor.cpp`
- `KTcpConnection.cpp`
- `KChannel.h`

这几处都会变干净，LT/ET 差异下沉到底层 helper。

### 4.5 第五步：把 buffer 选择收成统一选择层

目标：

- 上层只依赖一个统一 buffer 入口
- ringbuffer 与线性 buffer 的差异只留在实现层

建议改法：

1. 提供统一选择头，例如 `KSelectedBuffer.h`
2. 给两种 buffer 补齐统一能力接口，例如：
   - `readableView()`
   - `readableStringUntil()`
   - `isReadableContiguous()`
   - `retrieveLineAndCRLF()`

收益：

- `KHttpContext.cpp` 能用统一解析流程
- `KTcpConnection::send(Buffer*)` 能少一段分支

### 4.6 第六步：替换 stdout 日志

目标：

- 日志不再直接阻塞业务线程

建议改法：

- 引入异步文件 logger
- 默认关闭，按编译开关或环境变量启用
- 保留统一日志宏接口

收益：

- 性能路径更稳
- 头文件依赖更轻
- 运维使用体验更好

## 5. 落地顺序建议

推荐顺序：

1. 配置中心
2. pending functor 队列策略
3. recycle 生命周期策略
4. LT/ET IO helper
5. SelectedBuffer + Buffer 公共接口
6. 异步日志

原因很简单：

- 前两步主要是“收宏”和“降耦合”
- 中间两步处理最容易出错的连接和 IO 行为
- 最后处理日志和 buffer 统一接口时，改动面虽然大，但结构已经稳定

## 6. 测试与回归建议

每次做结构性重构时，至少覆盖下面几组构建：

```bash
make -j4
make USE_EPOLL_LT=1 -j4
make USE_RINGBUFFER=1 -j4
make USE_EPOLL_LT=1 USE_RINGBUFFER=1 -j4
make USE_RECYCLE=1 -j4
make USE_SPINLOCK=1 -j4
make USE_LOCKFREEQUEUE=1 -j4
```

还应至少手工验证：

- `runHttpServer` 能启动
- `/hello`、`/good`、`/file` 路径行为正常
- 长连接多次请求不串包
- 连接关闭后不会出现 double close / use-after-free

## 7. 结论

当前代码的优点是主干非常清晰：

- `EventLoop + Poller + Channel` 是标准 Reactor 核心
- `TcpServer + TcpConnection` 划分自然
- `HttpContext + HttpRequest + HttpResponse` 形成了完整的应用层链路

当前代码的主要问题不在“功能缺失”，而在“策略实现已经渗透到业务对象中”。  
因此后续最有价值的工作，不是继续堆功能，而是把这些策略层抽出来，让：

- 业务对象更专注
- 宏更集中
- 测试维度更清晰
- 以后继续优化性能时不需要反复碰主链路
