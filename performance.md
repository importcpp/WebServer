# 性能记录

## 测试日期

2026-04-14

## 测试范围

本文记录当前 `KTcpServer` 实现的一次本地 benchmark 结果。

本次测试 **不经过 HTTP 解析路径**，目标是测试 TCP 核心链路：

- `TcpServer`
- `TcpConnection`
- `Buffer`
- 事件循环 / poller / 线程池分发

benchmark 通过一个直接构建在 `KTcpServer` 之上的 echo server 进行压测，因此更适合作为传输层基线，而不是 HTTP 层 QPS 数据。

Benchmark 源文件：

- [`webserver/benchmark_kTcpServer.cpp`](./webserver/benchmark_kTcpServer.cpp)

## 测试环境

- Repository: `WebServer`
- Compiler: `g++ 10.2.1`
- 构建时安装的依赖: `boost-devel`
- 测试方式: localhost `127.0.0.1`
- Benchmark 可执行文件: `webserver/benchmark_kTcpServer`

## 测试方法

- benchmark 会先启动一个基于 `KTcpServer` 的本地 echo server。
- 多个 client 线程通过 TCP 建立连接，并在整个测试过程中保持长连接。
- 只有当所有 client 都连接完成并进入 ready 状态后，才开始计时。
- 每次请求发送固定大小的 payload，并等待服务端原样回显后再进入下一轮。
- 因此这里测到的是稳定态下的 request/response throughput，而不是建连成本。

## 编译命令

在 `webserver/` 目录下执行：

```bash
g++ -I . -O2 -std=c++11 -D NDEBUG runHttpServer.cpp \
  loop/KEventLoop.cpp loop/KEventLoopThread.cpp loop/KEventLoopThreadPool.cpp loop/KAsyncWaker.cpp \
  thread/KThreadPool.cpp poller/KChannel.cpp poller/KEventManager.cpp utils/KTimestamp.cpp \
  tcp/KSocket.cpp tcp/KSocketsOps.cpp tcp/KInetAddress.cpp tcp/KAcceptor.cpp tcp/KTcpServer.cpp tcp/KTcpConnection.cpp tcp/KBuffer.cpp tcp/KRingBuffer.cpp \
  http/KHttpResponse.cpp http/KHttpContext.cpp http/KHttpServer.cpp http/KIcons.cpp \
  -o runHttpServer -lpthread

g++ -I . -O2 -std=c++11 -D NDEBUG benchmark_kTcpServer.cpp \
  loop/KEventLoop.cpp loop/KEventLoopThread.cpp loop/KEventLoopThreadPool.cpp loop/KAsyncWaker.cpp \
  thread/KThreadPool.cpp poller/KChannel.cpp poller/KEventManager.cpp utils/KTimestamp.cpp \
  tcp/KSocket.cpp tcp/KSocketsOps.cpp tcp/KInetAddress.cpp tcp/KAcceptor.cpp tcp/KTcpServer.cpp tcp/KTcpConnection.cpp tcp/KBuffer.cpp tcp/KRingBuffer.cpp \
  http/KHttpResponse.cpp http/KHttpContext.cpp http/KHttpServer.cpp http/KIcons.cpp \
  -o benchmark_kTcpServer -lpthread
```

## 执行命令

```bash
./benchmark_kTcpServer --io-threads 3 --client-threads 8 --requests-per-thread 20000 --payload-size 64
./benchmark_kTcpServer --io-threads 3 --client-threads 8 --requests-per-thread 20000 --payload-size 1024
```

## 测试结果

| io_threads | client_threads | requests_per_thread | payload_size | total_requests | elapsed_seconds | throughput_req_per_sec | throughput_mib_per_sec | avg_round_trip_us_per_connection |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 3 | 8 | 20000 | 64 B | 160000 | 0.45 | 357486.96 | 21.82 | 22.38 |
| 3 | 8 | 20000 | 1024 B | 160000 | 0.47 | 339615.77 | 331.66 | 23.56 |

## 原始输出

### 64-byte payload

```text
Benchmark: KTcpServer echo round-trip
  port: 18888
  io_threads: 3
  client_threads: 8
  requests_per_thread: 20000
  payload_size_bytes: 64
Results:
  total_requests: 160000
  total_bytes: 10240000
  elapsed_seconds: 0.45
  throughput_req_per_sec: 357486.96
  throughput_mib_per_sec: 21.82
  avg_round_trip_us_per_connection: 22.38
Server stats:
  accepted_connections: 8
  closed_connections: 8
  echoed_messages: 160000
  echoed_bytes: 10240000
```

### 1024-byte payload

```text
Benchmark: KTcpServer echo round-trip
  port: 18888
  io_threads: 3
  client_threads: 8
  requests_per_thread: 20000
  payload_size_bytes: 1024
Results:
  total_requests: 160000
  total_bytes: 163840000
  elapsed_seconds: 0.47
  throughput_req_per_sec: 339615.77
  throughput_mib_per_sec: 331.66
  avg_round_trip_us_per_connection: 23.56
Server stats:
  accepted_connections: 8
  closed_connections: 8
  echoed_messages: 160000
  echoed_bytes: 163840000
```

## 说明

- 这些数据来自当前机器上的 localhost loopback，只适合作为本机基线，不等价于跨主机网络环境下的真实表现。
- 这个 benchmark 有意绕过了 HTTP 层，因此结果应理解为 `KTcpServer` 的传输层能力，而不是完整 webserver 的 HTTP QPS。
- 建连过程不计入最终统计时间窗口。
