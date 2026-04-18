cmake_minimum_required(VERSION 3.16)

project(WebServer
  VERSION 0.2.0
  DESCRIPTION "A reactor-based C++ web server"
  LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(WEBSERVER_BUILD_EXAMPLES "Build example executables" ON)
option(WEBSERVER_BUILD_BENCHMARKS "Build benchmark executables" ON)
option(WEBSERVER_BUILD_TESTS "Build test executables" ON)
option(WEBSERVER_USE_EPOLL_LT "Use level-triggered epoll instead of edge-triggered mode" OFF)
option(WEBSERVER_USE_RINGBUFFER "Use the ring-buffer implementation" OFF)
option(WEBSERVER_USE_LOCKFREEQUEUE "Use the lock-free functor queue" OFF)
option(WEBSERVER_USE_SPINLOCK "Use spinlocks for pending functor protection" OFF)
option(WEBSERVER_ENABLE_CONNECTION_RECYCLE "Reuse TcpConnection instances after close" OFF)
option(WEBSERVER_ENABLE_STDOUT_LOGGING "Enable verbose stdout logging macros" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WEBSERVER_PUBLIC_HEADERS
  webserver/http/KHttpContext.h
  webserver/http/KHttpRequest.h
  webserver/http/KHttpResponse.h
  webserver/http/KHttpServer.h
  webserver/http/KStaticFileCache.h
  webserver/lock/KLockFreeQueue.h
  webserver/lock/KSpinLock.h
  webserver/loop/KAsyncWaker.h
  webserver/loop/KEventLoop.h
  webserver/loop/KEventLoopThread.h
  webserver/loop/KEventLoopThreadPool.h
  webserver/poller/KChannel.h
  webserver/poller/KEventManager.h
  webserver/tcp/KAcceptor.h
  webserver/tcp/KBuffer.h
  webserver/tcp/KInetAddress.h
  webserver/tcp/KRingBuffer.h
  webserver/tcp/KSocket.h
  webserver/tcp/KSocketsOps.h
  webserver/tcp/KTcpConnection.h
  webserver/tcp/KTcpServer.h
  webserver/thread/KThreadPool.h
  webserver/timer/KTimer.h
  webserver/utils/KCallbacks.h
  webserver/utils/Kcopyable.h
  webserver/utils/Knoncopyable.h
  webserver/utils/KTimestamp.h
  webserver/utils/KTypes.h)

set(WEBSERVER_CORE_SOURCES
  webserver/loop/KAsyncWaker.cpp
  webserver/loop/KEventLoop.cpp
  webserver/loop/KEventLoopThread.cpp
  webserver/loop/KEventLoopThreadPool.cpp
  webserver/poller/KChannel.cpp
  webserver/poller/KEventManager.cpp
  webserver/tcp/KAcceptor.cpp
  webserver/tcp/KBuffer.cpp
  webserver/tcp/KInetAddress.cpp
  webserver/tcp/KRingBuffer.cpp
  webserver/tcp/KSocket.cpp
  webserver/tcp/KSocketsOps.cpp
  webserver/tcp/KTcpConnection.cpp
  webserver/tcp/KTcpServer.cpp
  webserver/thread/KThreadPool.cpp
  webserver/timer/KTimer.cpp
  webserver/utils/KTimestamp.cpp)

set(WEBSERVER_HTTP_SOURCES
  webserver/http/KHttpContext.cpp
  webserver/http/KHttpResponse.cpp
  webserver/http/KHttpServer.cpp
  webserver/http/KIcons.cpp
  webserver/http/KStaticFileCache.cpp)

function(webserver_apply_target_settings target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_17)
  target_include_directories(${target_name}
    PUBLIC
      $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  target_compile_definitions(${target_name}
    PUBLIC
      $<$<BOOL:${WEBSERVER_USE_EPOLL_LT}>:USE_EPOLL_LT>
      $<$<BOOL:${WEBSERVER_USE_RINGBUFFER}>:USE_RINGBUFFER>
      $<$<BOOL:${WEBSERVER_USE_LOCKFREEQUEUE}>:USE_LOCKFREEQUEUE>
      $<$<BOOL:${WEBSERVER_USE_SPINLOCK}>:USE_SPINLOCK>
      $<$<BOOL:${WEBSERVER_ENABLE_CONNECTION_RECYCLE}>:USE_RECYCLE>
      $<$<BOOL:${WEBSERVER_ENABLE_STDOUT_LOGGING}>:USE_STD_COUT>)
endfunction()

add_library(webserver_core ${WEBSERVER_CORE_SOURCES} ${WEBSERVER_PUBLIC_HEADERS})
add_library(WebServer::core ALIAS webserver_core)
webserver_apply_target_settings(webserver_core)
target_link_libraries(webserver_core PUBLIC pthread)

add_library(webserver_http ${WEBSERVER_HTTP_SOURCES})
add_library(WebServer::http ALIAS webserver_http)
webserver_apply_target_settings(webserver_http)
target_link_libraries(webserver_http PUBLIC webserver_core)

if(WEBSERVER_BUILD_EXAMPLES)
  add_executable(run_http_server webserver/runHttpServer.cpp)
  webserver_apply_target_settings(run_http_server)
  target_link_libraries(run_http_server PRIVATE webserver_http)
endif()

if(WEBSERVER_BUILD_BENCHMARKS)
  add_executable(benchmark_ktcpserver webserver/benchmark_kTcpServer.cpp)
  webserver_apply_target_settings(benchmark_ktcpserver)
  target_link_libraries(benchmark_ktcpserver PRIVATE webserver_core)
endif()

if(WEBSERVER_BUILD_TESTS)
  enable_testing()

  add_executable(kthreadpool_test webserver/thread/test_kthreadpool.cpp)
  webserver_apply_target_settings(kthreadpool_test)
  target_link_libraries(kthreadpool_test PRIVATE webserver_core)
  add_test(NAME kthreadpool_test COMMAND kthreadpool_test)
endif()

install(
  TARGETS webserver_core webserver_http
  EXPORT WebServerTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(WEBSERVER_BUILD_EXAMPLES)
  install(TARGETS run_http_server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(WEBSERVER_BUILD_BENCHMARKS)
  install(TARGETS benchmark_ktcpserver RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(
  DIRECTORY webserver/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/webserver
  FILES_MATCHING PATTERN "*.h")

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/WebServerConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

configure_package_config_file(
  ${PROJECT_SOURCE_DIR}/cmake/WebServerConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/WebServerConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/WebServer)

install(
  EXPORT WebServerTargets
  NAMESPACE WebServer::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/WebServer)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/WebServerConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/WebServerConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/WebServer)
