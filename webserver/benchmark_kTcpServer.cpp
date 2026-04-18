#include "webserver/loop/KEventLoop.h"
#include "webserver/tcp/KTcpServer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace kback;

namespace {

struct BenchmarkConfig {
  int port = 18888;
  int io_threads = 3;
  int client_threads = 8;
  int requests_per_thread = 20000;
  int payload_size = 64;
};

struct ServerStats {
  std::atomic<uint64_t> accepted{0};
  std::atomic<uint64_t> closed{0};
  std::atomic<uint64_t> echoed_messages{0};
  std::atomic<uint64_t> echoed_bytes{0};
};

struct WorkerResult {
  uint64_t requests = 0;
  uint64_t bytes = 0;
};

class StartGate {
public:
  explicit StartGate(int expected) : expected_(expected) {}

  void arriveAndWait() {
    std::unique_lock<std::mutex> lock(mutex_);
    ++ready_;
    readyCv_.notify_one();
    startCv_.wait(lock, [this] { return started_ || aborted_; });
  }

  void waitUntilAllReady() {
    std::unique_lock<std::mutex> lock(mutex_);
    readyCv_.wait(lock, [this] { return ready_ == expected_ || aborted_; });
  }

  void releaseAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = true;
    startCv_.notify_all();
  }

  void abort() {
    std::lock_guard<std::mutex> lock(mutex_);
    aborted_ = true;
    started_ = true;
    readyCv_.notify_one();
    startCv_.notify_all();
  }

private:
  const int expected_;
  int ready_ = 0;
  bool started_ = false;
  bool aborted_ = false;
  std::mutex mutex_;
  std::condition_variable readyCv_;
  std::condition_variable startCv_;
};

int parsePositiveInt(const char *value, const char *name) {
  char *end = nullptr;
  long parsed = std::strtol(value, &end, 10);
  if (end == value || *end != '\0' || parsed <= 0) {
    throw std::runtime_error(std::string("invalid value for ") + name + ": " +
                             value);
  }
  if (parsed > INT32_MAX) {
    throw std::runtime_error(std::string("value too large for ") + name + ": " +
                             value);
  }
  return static_cast<int>(parsed);
}

void printUsage(const char *argv0) {
  std::cout << "Usage: " << argv0
            << " [--port N] [--io-threads N] [--client-threads N]"
               " [--requests-per-thread N] [--payload-size N]\n";
}

BenchmarkConfig parseArgs(int argc, char *argv[]) {
  BenchmarkConfig config;
  for (int i = 1; i < argc; ++i) {
    const std::string arg(argv[i]);
    if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      std::exit(0);
    } else if (arg == "--port" && i + 1 < argc) {
      config.port = parsePositiveInt(argv[++i], "--port");
      if (config.port > 65535) {
        throw std::runtime_error("port must be <= 65535");
      }
    } else if (arg == "--io-threads" && i + 1 < argc) {
      config.io_threads = parsePositiveInt(argv[++i], "--io-threads");
    } else if (arg == "--client-threads" && i + 1 < argc) {
      config.client_threads = parsePositiveInt(argv[++i], "--client-threads");
    } else if (arg == "--requests-per-thread" && i + 1 < argc) {
      config.requests_per_thread =
          parsePositiveInt(argv[++i], "--requests-per-thread");
    } else if (arg == "--payload-size" && i + 1 < argc) {
      config.payload_size = parsePositiveInt(argv[++i], "--payload-size");
    } else {
      throw std::runtime_error("unknown or incomplete argument: " + arg);
    }
  }
  return config;
}

void failErrno(const std::string &message) {
  throw std::runtime_error(message + ": " + std::strerror(errno));
}

void writeFully(int fd, const char *data, size_t len) {
  while (len > 0) {
    const ssize_t n = ::send(fd, data, len, 0);
    if (n > 0) {
      data += n;
      len -= static_cast<size_t>(n);
      continue;
    }
    if (n < 0 && errno == EINTR) {
      continue;
    }
    failErrno("send failed");
  }
}

void readFully(int fd, char *data, size_t len) {
  while (len > 0) {
    const ssize_t n = ::recv(fd, data, len, 0);
    if (n > 0) {
      data += n;
      len -= static_cast<size_t>(n);
      continue;
    }
    if (n == 0) {
      throw std::runtime_error("server closed connection unexpectedly");
    }
    if (errno == EINTR) {
      continue;
    }
    failErrno("recv failed");
  }
}

int connectToLocalhost(int port) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) {
    failErrno("socket failed");
  }

  const int one = 1;
  if (::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0) {
    ::close(fd);
    failErrno("setsockopt TCP_NODELAY failed");
  }

  sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port));
  if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
    ::close(fd);
    throw std::runtime_error("inet_pton failed for 127.0.0.1");
  }

  if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    failErrno("connect failed");
  }

  return fd;
}

class EchoServer {
public:
  EchoServer(EventLoop *loop, int port, int ioThreads, ServerStats *stats)
      : server_(loop, InetAddress(static_cast<uint16_t>(port)),
                "ktcp-benchmark"),
        stats_(stats) {
    server_.setThreadNum(ioThreads);
    server_.setConnectionCallback(
        [this](const TcpConnectionPtr &conn) { onConnection(conn); });
    server_.setMessageCallback([this](const TcpConnectionPtr &conn, Buffer *buf,
                                      Timestamp receiveTime) {
      onMessage(conn, buf, receiveTime);
    });
  }

  void start() { server_.start(); }

private:
  void onConnection(const TcpConnectionPtr &conn) {
    if (conn->connected()) {
      conn->setTcpNoDelay(true);
      stats_->accepted.fetch_add(1, std::memory_order_relaxed);
    } else {
      stats_->closed.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp) {
    const size_t payloadSize = buf->readableBytes();
    if (payloadSize == 0) {
      return;
    }
    stats_->echoed_messages.fetch_add(1, std::memory_order_relaxed);
    stats_->echoed_bytes.fetch_add(payloadSize, std::memory_order_relaxed);
    conn->send(buf);
  }

  TcpServer server_;
  ServerStats *stats_;
};

class ServerRunner {
public:
  ServerRunner(int port, int ioThreads, ServerStats *stats)
      : port_(port), ioThreads_(ioThreads), stats_(stats) {}

  ~ServerRunner() { stop(); }

  void start() {
    thread_ = std::thread(&ServerRunner::threadMain, this);
    std::unique_lock<std::mutex> lock(mutex_);
    readyCv_.wait(lock, [this] { return ready_ || static_cast<bool>(error_); });
    if (error_) {
      std::rethrow_exception(error_);
    }
  }

  void stop() {
    EventLoop *loop = nullptr;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      loop = loop_;
    }
    if (loop != nullptr) {
      loop->quit();
    }
    if (thread_.joinable()) {
      thread_.join();
    }
  }

private:
  void threadMain() {
    try {
      EventLoop loop;
      EchoServer server(&loop, port_, ioThreads_, stats_);
      server.start();

      {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        ready_ = true;
      }
      readyCv_.notify_one();

      loop.loop();

      {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = nullptr;
        ready_ = false;
      }
    } catch (...) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        error_ = std::current_exception();
      }
      readyCv_.notify_one();
    }
  }

  const int port_;
  const int ioThreads_;
  ServerStats *stats_;
  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable readyCv_;
  EventLoop *loop_ = nullptr;
  bool ready_ = false;
  std::exception_ptr error_;
};

void runClientWorker(const BenchmarkConfig &config, StartGate *startGate,
                     WorkerResult *result) {
  const int fd = connectToLocalhost(config.port);
  std::vector<char> request(config.payload_size, 'x');
  std::vector<char> response(config.payload_size, 0);

  startGate->arriveAndWait();

  for (int i = 0; i < config.requests_per_thread; ++i) {
    writeFully(fd, request.data(), request.size());
    readFully(fd, response.data(), response.size());
  }

  ::close(fd);
  result->requests = static_cast<uint64_t>(config.requests_per_thread);
  result->bytes = result->requests * static_cast<uint64_t>(config.payload_size);
}

} // namespace

int main(int argc, char *argv[]) {
  try {
    const BenchmarkConfig config = parseArgs(argc, argv);
    ServerStats serverStats;
    ServerRunner server(config.port, config.io_threads, &serverStats);
    server.start();

    std::vector<std::thread> workers;
    std::vector<WorkerResult> results(
        static_cast<size_t>(config.client_threads));
    std::vector<std::exception_ptr> workerErrors(
        static_cast<size_t>(config.client_threads));
    StartGate startGate(config.client_threads);

    workers.reserve(static_cast<size_t>(config.client_threads));
    for (int i = 0; i < config.client_threads; ++i) {
      workers.emplace_back([&config, &startGate, &results, &workerErrors, i] {
        try {
          runClientWorker(config, &startGate, &results[static_cast<size_t>(i)]);
        } catch (...) {
          workerErrors[static_cast<size_t>(i)] = std::current_exception();
          startGate.abort();
        }
      });
    }

    startGate.waitUntilAllReady();
    const auto start = std::chrono::steady_clock::now();
    startGate.releaseAll();

    for (size_t i = 0; i < workers.size(); ++i) {
      workers[i].join();
    }
    const auto end = std::chrono::steady_clock::now();

    for (size_t i = 0; i < workerErrors.size(); ++i) {
      if (workerErrors[i]) {
        std::rethrow_exception(workerErrors[i]);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    server.stop();

    uint64_t totalRequests = 0;
    uint64_t totalBytes = 0;
    for (size_t i = 0; i < results.size(); ++i) {
      totalRequests += results[i].requests;
      totalBytes += results[i].bytes;
    }

    const std::chrono::duration<double> elapsed = end - start;
    const double seconds = elapsed.count();
    const double reqPerSec =
        seconds > 0.0 ? static_cast<double>(totalRequests) / seconds : 0.0;
    const double mibPerSec =
        seconds > 0.0 ? (static_cast<double>(totalBytes) / (1024.0 * 1024.0)) /
                            seconds
                      : 0.0;
    const double avgRoundTripUs =
        config.requests_per_thread > 0
            ? (seconds * 1000.0 * 1000.0) /
                  static_cast<double>(config.requests_per_thread)
            : 0.0;

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Benchmark: KTcpServer echo round-trip\n";
    std::cout << "  port: " << config.port << "\n";
    std::cout << "  io_threads: " << config.io_threads << "\n";
    std::cout << "  client_threads: " << config.client_threads << "\n";
    std::cout << "  requests_per_thread: " << config.requests_per_thread
              << "\n";
    std::cout << "  payload_size_bytes: " << config.payload_size << "\n";
    std::cout << "Results:\n";
    std::cout << "  total_requests: " << totalRequests << "\n";
    std::cout << "  total_bytes: " << totalBytes << "\n";
    std::cout << "  elapsed_seconds: " << seconds << "\n";
    std::cout << "  throughput_req_per_sec: " << reqPerSec << "\n";
    std::cout << "  throughput_mib_per_sec: " << mibPerSec << "\n";
    std::cout << "  avg_round_trip_us_per_connection: " << avgRoundTripUs
              << "\n";
    std::cout << "Server stats:\n";
    std::cout << "  accepted_connections: "
              << serverStats.accepted.load(std::memory_order_relaxed) << "\n";
    std::cout << "  closed_connections: "
              << serverStats.closed.load(std::memory_order_relaxed) << "\n";
    std::cout << "  echoed_messages: "
              << serverStats.echoed_messages.load(std::memory_order_relaxed)
              << "\n";
    std::cout << "  echoed_bytes: "
              << serverStats.echoed_bytes.load(std::memory_order_relaxed)
              << "\n";
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << "benchmark failed: " << ex.what() << std::endl;
    return 1;
  }
}
