#pragma once

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <deque>
#include <vector>

class ThreadPool {
public:
  typedef std::function<void()> Task;

  explicit ThreadPool(const std::string &name = std::string());
  ~ThreadPool();

  void start(int numThreads);
  void stop();
  void run(const Task &f);

private:
  void runInThread();

  Task take();

  std::mutex mutex_;
  std::condition_variable cond_;
  std::string name_;
  std::vector<std::thread> threads_;
  std::deque<Task> queue_;
  bool running_;
};