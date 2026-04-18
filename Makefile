CXX ?= g++
AR ?= ar

MODE ?= release
BUILD_DIR ?= build

USE_EPOLL_LT ?= 0
USE_RINGBUFFER ?= 0
USE_LOCKFREEQUEUE ?= 0
USE_SPINLOCK ?= 0
USE_RECYCLE ?= 0
USE_STDOUT_LOGGING ?= 0

OBJ_DIR := $(BUILD_DIR)/obj
LIB_DIR := $(BUILD_DIR)/lib
BIN_DIR := $(BUILD_DIR)/bin

WARNINGS := -Wall -Wextra -Wpedantic
COMMON_CXXFLAGS := -std=c++17 -I . -MMD -MP $(WARNINGS)
CPPFLAGS += $(if $(filter 1,$(USE_EPOLL_LT)),-DUSE_EPOLL_LT,)
CPPFLAGS += $(if $(filter 1,$(USE_RINGBUFFER)),-DUSE_RINGBUFFER,)
CPPFLAGS += $(if $(filter 1,$(USE_LOCKFREEQUEUE)),-DUSE_LOCKFREEQUEUE,)
CPPFLAGS += $(if $(filter 1,$(USE_SPINLOCK)),-DUSE_SPINLOCK,)
CPPFLAGS += $(if $(filter 1,$(USE_RECYCLE)),-DUSE_RECYCLE,)
CPPFLAGS += $(if $(filter 1,$(USE_STDOUT_LOGGING)),-DUSE_STD_COUT,)

ifeq ($(MODE),debug)
  CXXFLAGS += $(COMMON_CXXFLAGS) -O0 -g3
else ifeq ($(MODE),release)
  CXXFLAGS += $(COMMON_CXXFLAGS) -O3 -DNDEBUG
else
  $(error Unsupported MODE '$(MODE)'; use MODE=release or MODE=debug)
endif

LDLIBS += -pthread

CORE_SRCS := \
	webserver/loop/KAsyncWaker.cpp \
	webserver/loop/KEventLoop.cpp \
	webserver/loop/KEventLoopThread.cpp \
	webserver/loop/KEventLoopThreadPool.cpp \
	webserver/poller/KChannel.cpp \
	webserver/poller/KEventManager.cpp \
	webserver/tcp/KAcceptor.cpp \
	webserver/tcp/KBuffer.cpp \
	webserver/tcp/KInetAddress.cpp \
	webserver/tcp/KRingBuffer.cpp \
	webserver/tcp/KSocket.cpp \
	webserver/tcp/KSocketsOps.cpp \
	webserver/tcp/KTcpConnection.cpp \
	webserver/tcp/KTcpServer.cpp \
	webserver/thread/KThreadPool.cpp \
	webserver/timer/KTimer.cpp \
	webserver/utils/KTimestamp.cpp

HTTP_SRCS := \
	webserver/http/KHttpContext.cpp \
	webserver/http/KHttpResponse.cpp \
	webserver/http/KHttpServer.cpp \
	webserver/http/KIcons.cpp \
	webserver/http/KStaticFileCache.cpp

RUN_HTTP_SERVER_SRCS := webserver/runHttpServer.cpp
BENCHMARK_SRCS := webserver/benchmark_kTcpServer.cpp
THREADPOOL_TEST_SRCS := webserver/thread/test_kthreadpool.cpp

CORE_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(CORE_SRCS))
HTTP_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(HTTP_SRCS))
RUN_HTTP_SERVER_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(RUN_HTTP_SERVER_SRCS))
BENCHMARK_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(BENCHMARK_SRCS))
THREADPOOL_TEST_OBJS := $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(THREADPOOL_TEST_SRCS))

CORE_LIB := $(LIB_DIR)/libwebserver_core.a
HTTP_LIB := $(LIB_DIR)/libwebserver_http.a
RUN_HTTP_SERVER_BIN := $(BIN_DIR)/runHttpServer
BENCHMARK_BIN := $(BIN_DIR)/benchmark_kTcpServer
THREADPOOL_TEST_BIN := $(BIN_DIR)/test_kthreadpool

ALL_OBJS := $(CORE_OBJS) $(HTTP_OBJS) $(RUN_HTTP_SERVER_OBJS) \
	$(BENCHMARK_OBJS) $(THREADPOOL_TEST_OBJS)
ALL_DEPS := $(ALL_OBJS:.o=.d)

.PHONY: all clean print-config runHttpServer benchmark_kTcpServer test_kthreadpool

all: $(RUN_HTTP_SERVER_BIN) $(BENCHMARK_BIN) $(THREADPOOL_TEST_BIN)

print-config:
	@printf 'MODE=%s\n' "$(MODE)"
	@printf 'BUILD_DIR=%s\n' "$(BUILD_DIR)"
	@printf 'USE_EPOLL_LT=%s\n' "$(USE_EPOLL_LT)"
	@printf 'USE_RINGBUFFER=%s\n' "$(USE_RINGBUFFER)"
	@printf 'USE_LOCKFREEQUEUE=%s\n' "$(USE_LOCKFREEQUEUE)"
	@printf 'USE_SPINLOCK=%s\n' "$(USE_SPINLOCK)"
	@printf 'USE_RECYCLE=%s\n' "$(USE_RECYCLE)"
	@printf 'USE_STDOUT_LOGGING=%s\n' "$(USE_STDOUT_LOGGING)"

runHttpServer: $(RUN_HTTP_SERVER_BIN)

benchmark_kTcpServer: $(BENCHMARK_BIN)

test_kthreadpool: $(THREADPOOL_TEST_BIN)

$(CORE_LIB): $(CORE_OBJS) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(HTTP_LIB): $(HTTP_OBJS) $(CORE_LIB) | $(LIB_DIR)
	$(AR) rcs $@ $(HTTP_OBJS)

$(RUN_HTTP_SERVER_BIN): $(RUN_HTTP_SERVER_OBJS) $(HTTP_LIB) $(CORE_LIB) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) -o $@ $(RUN_HTTP_SERVER_OBJS) $(HTTP_LIB) $(CORE_LIB) $(LDLIBS)

$(BENCHMARK_BIN): $(BENCHMARK_OBJS) $(CORE_LIB) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) -o $@ $(BENCHMARK_OBJS) $(CORE_LIB) $(LDLIBS)

$(THREADPOOL_TEST_BIN): $(THREADPOOL_TEST_OBJS) $(CORE_LIB) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) -o $@ $(THREADPOOL_TEST_OBJS) $(CORE_LIB) $(LDLIBS)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(LIB_DIR) $(BIN_DIR):
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

-include $(ALL_DEPS)
