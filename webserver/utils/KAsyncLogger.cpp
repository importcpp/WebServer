#include "KAsyncLogger.h"

#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace kback {
namespace {

constexpr size_t kMaxLogLineBytes = 1024;
constexpr size_t kInitialBufferedRecords = 1024;
constexpr size_t kMaxBufferedRecords = 16384;
constexpr size_t kNotifyThreshold = 256;
constexpr auto kFlushInterval = std::chrono::milliseconds(500);

struct LogRecord {
  char data[kMaxLogLineBytes];
  size_t size;
};

const char *defaultLogFilePath() {
  const char *envPath = std::getenv("WEBSERVER_LOG_FILE");
  if (envPath != nullptr && envPath[0] != '\0') {
    return envPath;
  }
  return "webserver.log";
}

const char *basenameOf(const char *path) {
  if (path == nullptr) {
    return "unknown";
  }
  const char *slash = std::strrchr(path, '/');
  return slash == nullptr ? path : slash + 1;
}

const char *levelToString(LogLevel level) {
  switch (level) {
  case LogLevel::kTrace:
    return "TRACE";
  case LogLevel::kDebug:
    return "DEBUG";
  case LogLevel::kInfo:
    return "INFO";
  case LogLevel::kWarn:
    return "WARN";
  case LogLevel::kError:
    return "ERROR";
  case LogLevel::kFatal:
    return "FATAL";
  case LogLevel::kSysErr:
    return "SYSERR";
  case LogLevel::kSysFatal:
    return "SYSFATAL";
  }
  return "UNKNOWN";
}

unsigned long currentThreadId() {
  return static_cast<unsigned long>(::syscall(SYS_gettid));
}

void writeAll(int fd, const char *data, size_t size) {
  while (size > 0) {
    const ssize_t written = ::write(fd, data, size);
    if (written > 0) {
      data += written;
      size -= static_cast<size_t>(written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
}

size_t appendFormatted(char *buffer, size_t capacity, size_t offset,
                       const char *format, va_list args) {
  if (offset >= capacity) {
    return capacity;
  }
  const int written =
      std::vsnprintf(buffer + offset, capacity - offset, format, args);
  if (written < 0) {
    return offset;
  }
  const size_t advanced = static_cast<size_t>(written);
  if (offset + advanced >= capacity) {
    return capacity - 1;
  }
  return offset + advanced;
}

LogRecord formatLogRecord(LogLevel level, const char *file, int line,
                          int savedErrno, const char *format, va_list args) {
  LogRecord record{};
  char timestamp[64];
  struct timespec ts;
  ::clock_gettime(CLOCK_REALTIME, &ts);
  struct tm localTime;
  ::localtime_r(&ts.tv_sec, &localTime);
  std::snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                localTime.tm_year + 1900, localTime.tm_mon + 1,
                localTime.tm_mday, localTime.tm_hour, localTime.tm_min,
                localTime.tm_sec);

  size_t offset = static_cast<size_t>(std::snprintf(
      record.data, sizeof(record.data),
      "%s.%06ld [%s] [tid=%lu] %s:%d ", timestamp, ts.tv_nsec / 1000,
      levelToString(level), currentThreadId(), basenameOf(file), line));

  if (offset >= sizeof(record.data)) {
    offset = sizeof(record.data) - 1;
  }

  offset = appendFormatted(record.data, sizeof(record.data), offset, format,
                           args);

  if (savedErrno != 0 && offset < sizeof(record.data)) {
    const char *errorString = std::strerror(savedErrno);
    const int written = std::snprintf(record.data + offset,
                                      sizeof(record.data) - offset,
                                      " (errno=%d: %s)", savedErrno,
                                      errorString == nullptr ? "unknown"
                                                             : errorString);
    if (written > 0) {
      offset += static_cast<size_t>(written);
      if (offset >= sizeof(record.data)) {
        offset = sizeof(record.data) - 1;
      }
    }
  }

  if (offset >= sizeof(record.data) - 1) {
    offset = sizeof(record.data) - 2;
  }
  record.data[offset++] = '\n';
  record.data[offset] = '\0';
  record.size = offset;
  return record;
}

class AsyncFileLogger {
public:
  AsyncFileLogger()
      : logFilePath_(defaultLogFilePath()), fd_(-1), running_(false),
        stopRequested_(false), droppedMessages_(0) {
    pending_.reserve(kInitialBufferedRecords);
  }

  ~AsyncFileLogger() { shutdown(); }

  void setLogFile(std::string_view path) {
    if (path.empty()) {
      return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (running_ || fd_ >= 0) {
      return;
    }
    logFilePath_.assign(path.data(), path.size());
  }

  void append(LogLevel level, const char *file, int line, int savedErrno,
              const char *format, va_list args) {
    LogRecord record = formatLogRecord(level, file, line, savedErrno, format,
                                       args);

    if (level == LogLevel::kFatal || level == LogLevel::kSysFatal) {
      directWrite(record);
      return;
    }

    bool shouldNotify = false;
    bool shouldFallback = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      startLocked();
      if (!running_) {
        shouldFallback = true;
      } else if (pending_.size() >= kMaxBufferedRecords) {
        ++droppedMessages_;
      } else {
        pending_.push_back(record);
        shouldNotify = pending_.size() == 1 || pending_.size() >= kNotifyThreshold;
      }
    }

    if (shouldFallback) {
      directWrite(record);
      return;
    }

    if (shouldNotify) {
      cv_.notify_one();
    }
  }

  void shutdown() {
    std::thread worker;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!running_) {
        if (fd_ >= 0) {
          ::close(fd_);
          fd_ = -1;
        }
        return;
      }
      stopRequested_ = true;
      cv_.notify_one();
      worker = std::move(worker_);
    }

    if (worker.joinable()) {
      worker.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    stopRequested_ = false;
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

private:
  void startLocked() {
    if (running_) {
      return;
    }

    openLocked();
    if (fd_ < 0) {
      return;
    }

    stopRequested_ = false;
    running_ = true;
    worker_ = std::thread([this] { workerLoop(); });
  }

  void openLocked() {
    if (fd_ >= 0) {
      return;
    }

    fd_ = ::open(logFilePath_.c_str(),
                 O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  }

  void workerLoop() {
    std::vector<LogRecord> localRecords;
    localRecords.reserve(kInitialBufferedRecords);

    for (;;) {
      uint64_t dropped = 0;
      bool shouldStop = false;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock, kFlushInterval,
                     [this] { return stopRequested_ || !pending_.empty(); });
        pending_.swap(localRecords);
        dropped = droppedMessages_;
        droppedMessages_ = 0;
        shouldStop = stopRequested_ && localRecords.empty() && dropped == 0;
      }

      if (shouldStop) {
        return;
      }

      if (dropped > 0) {
        LogRecord dropRecord{};
        const int written = std::snprintf(
            dropRecord.data, sizeof(dropRecord.data),
            "logger dropped %" PRIu64 " messages because the async queue was "
            "full\n",
            dropped);
        if (written > 0) {
          dropRecord.size = static_cast<size_t>(written);
          writeRecord(dropRecord);
        }
      }

      for (const LogRecord &record : localRecords) {
        writeRecord(record);
      }
      localRecords.clear();
    }
  }

  void directWrite(const LogRecord &record) {
    int targetFd = -1;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      openLocked();
      targetFd = fd_;
    }

    if (targetFd >= 0) {
      writeAll(targetFd, record.data, record.size);
      return;
    }

    writeAll(STDERR_FILENO, record.data, record.size);
  }

  void writeRecord(const LogRecord &record) {
    int targetFd = -1;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      targetFd = fd_;
    }

    if (targetFd >= 0) {
      writeAll(targetFd, record.data, record.size);
      return;
    }

    writeAll(STDERR_FILENO, record.data, record.size);
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::vector<LogRecord> pending_;
  std::string logFilePath_;
  int fd_;
  std::thread worker_;
  bool running_;
  bool stopRequested_;
  uint64_t droppedMessages_;
};

AsyncFileLogger &GetLogger() {
  static AsyncFileLogger logger;
  return logger;
}

} // namespace

void SetAsyncLogFile(std::string_view path) { GetLogger().setLogFile(path); }

void ShutdownAsyncLogger() { GetLogger().shutdown(); }

namespace detail {

void Log(LogLevel level, const char *file, int line, int savedErrno,
         const char *format, ...) {
  va_list args;
  va_start(args, format);
  GetLogger().append(level, file, line, savedErrno, format, args);
  va_end(args);
}

} // namespace detail
} // namespace kback
