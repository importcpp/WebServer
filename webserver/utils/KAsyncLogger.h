#pragma once

#include <cerrno>
#include <string_view>

namespace kback {

enum class LogLevel : unsigned char {
  kTrace,
  kDebug,
  kInfo,
  kWarn,
  kError,
  kFatal,
  kSysErr,
  kSysFatal
};

void SetAsyncLogFile(std::string_view path);
void ShutdownAsyncLogger();

namespace detail {

void Log(LogLevel level, const char *file, int line, int savedErrno,
         const char *format, ...);

} // namespace detail

} // namespace kback

#if defined(USE_ASYNC_FILE_LOGGING)
#define KBACK_LOG_TRACE(...)                                                   \
  ::kback::detail::Log(::kback::LogLevel::kTrace, __FILE__, __LINE__, 0,       \
                       __VA_ARGS__)
#define KBACK_LOG_DEBUG(...)                                                   \
  ::kback::detail::Log(::kback::LogLevel::kDebug, __FILE__, __LINE__, 0,       \
                       __VA_ARGS__)
#define KBACK_LOG_INFO(...)                                                    \
  ::kback::detail::Log(::kback::LogLevel::kInfo, __FILE__, __LINE__, 0,        \
                       __VA_ARGS__)
#define KBACK_LOG_WARN(...)                                                    \
  ::kback::detail::Log(::kback::LogLevel::kWarn, __FILE__, __LINE__, 0,        \
                       __VA_ARGS__)
#define KBACK_LOG_ERROR(...)                                                   \
  ::kback::detail::Log(::kback::LogLevel::kError, __FILE__, __LINE__, 0,       \
                       __VA_ARGS__)
#define KBACK_LOG_FATAL(...)                                                   \
  ::kback::detail::Log(::kback::LogLevel::kFatal, __FILE__, __LINE__, 0,       \
                       __VA_ARGS__)
#define KBACK_LOG_SYSERR(...)                                                  \
  ::kback::detail::Log(::kback::LogLevel::kSysErr, __FILE__, __LINE__, errno,  \
                       __VA_ARGS__)
#define KBACK_LOG_SYSFATAL(...)                                                \
  ::kback::detail::Log(::kback::LogLevel::kSysFatal, __FILE__, __LINE__,       \
                       errno, __VA_ARGS__)
#else
#define KBACK_LOG_TRACE(...) ((void)0)
#define KBACK_LOG_DEBUG(...) ((void)0)
#define KBACK_LOG_INFO(...) ((void)0)
#define KBACK_LOG_WARN(...) ((void)0)
#define KBACK_LOG_ERROR(...) ((void)0)
#define KBACK_LOG_FATAL(...) ((void)0)
#define KBACK_LOG_SYSERR(...) ((void)0)
#define KBACK_LOG_SYSFATAL(...) ((void)0)
#endif
