#pragma once

#include "KRecycledConnectionPool.h"
#include "KTcpConnection.h"
#include "webserver/utils/KBuildConfig.h"

#include <atomic>
#include <type_traits>
#include <utility>

namespace kback {

template <typename ConnectionPtr> class DisabledTcpConnectionRecycler {
public:
  bool tryTake(ConnectionPtr &conn) {
    (void)conn;
    return false;
  }

  void recycle(ConnectionPtr conn) { (void)conn; }
};

template <typename ConnectionPtr> class EnabledTcpConnectionRecycler {
public:
  bool tryTake(ConnectionPtr &conn) { return recycledConnections_.tryTake(conn); }

  void recycle(ConnectionPtr conn) {
    if ((recycleCounter_.fetch_add(1, std::memory_order_relaxed) & 1U) == 0U) {
      return;
    }
    recycledConnections_.push(std::move(conn));
  }

private:
  std::atomic<unsigned int> recycleCounter_{0};
  RecycledConnectionPool<ConnectionPtr> recycledConnections_;
};

template <typename ConnectionPtr>
using TcpConnectionRecycler =
    std::conditional_t<kEnableConnectionRecycle,
                       EnabledTcpConnectionRecycler<ConnectionPtr>,
                       DisabledTcpConnectionRecycler<ConnectionPtr>>;

} // namespace kback
