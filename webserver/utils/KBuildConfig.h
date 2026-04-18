#pragma once

namespace kback {

#ifdef USE_RECYCLE
inline constexpr bool kEnableConnectionRecycle = true;
#else
inline constexpr bool kEnableConnectionRecycle = false;
#endif

#ifdef USE_EPOLL_LT
inline constexpr bool kUseEpollLT = true;
#else
inline constexpr bool kUseEpollLT = false;
#endif

#ifdef USE_RINGBUFFER
inline constexpr bool kUseRingBuffer = true;
#else
inline constexpr bool kUseRingBuffer = false;
#endif

} // namespace kback
