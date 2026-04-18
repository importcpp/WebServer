#pragma once

#include "webserver/utils/KBuildConfig.h"

#include <type_traits>

namespace kback {

class TcpConnection;

class DisabledTcpConnectionLifecycle {
public:
  void afterConnectDestroyed(TcpConnection &connection) const;
};

class EnabledTcpConnectionLifecycle {
public:
  void afterConnectDestroyed(TcpConnection &connection) const;
};

using TcpConnectionLifecycle =
    std::conditional_t<kEnableConnectionRecycle, EnabledTcpConnectionLifecycle,
                       DisabledTcpConnectionLifecycle>;

} // namespace kback
