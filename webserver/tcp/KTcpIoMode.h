#pragma once

#include "webserver/utils/KAsyncLogger.h"
#include "webserver/utils/KBuildConfig.h"

#include <errno.h>
#include <string_view>
#include <unistd.h>

namespace kback::tcp_io_mode {

inline bool acceptOneConnectionPerEvent() { return kUseEpollLT; }

template <typename Buffer>
ssize_t read(Buffer &buffer, int fd, int *savedErrno) {
  if constexpr (kUseEpollLT) {
    return buffer.readFd(fd, savedErrno);
  } else {
    return buffer.readFdET(fd, savedErrno);
  }
}

template <typename Buffer>
ssize_t write(Buffer &buffer, int fd, int *savedErrno) {
  if constexpr (kUseEpollLT) {
    return buffer.writeFd(fd, savedErrno);
  } else {
    return buffer.writeFdET(fd, savedErrno);
  }
}

inline ssize_t writeDirect(int fd, std::string_view message) {
  if constexpr (kUseEpollLT) {
    return ::write(fd, message.data(), message.size());
  } else {
    ssize_t writesum = 0;
    const char *begin = message.data();
    size_t len = message.size();
    for (;;) {
      const ssize_t n = ::write(fd, begin, len);
      if (n > 0) {
        writesum += n;
        begin += n;
        len -= static_cast<size_t>(n);
        if (len == 0) {
          return writesum;
        }
      } else if (n < 0) {
        if (errno == EAGAIN) {
          KBACK_LOG_TRACE("ET mode: errno == EAGAIN");
          break;
        }
        return -1;
      } else {
        return 0;
      }
    }
    return writesum;
  }
}

} // namespace kback::tcp_io_mode
