#include "KThreadPool.h"
#include "assert.h"

using namespace kb;

ThreadPool::ThreadPool(const std::string &name)
    : name_(name),
      running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads);
  for (size_t i = 0; i < numThreads; ++i)
  {
    threads_.push_back(std::thread(
        std::bind(&ThreadPool::runInThread, this)));
  }
}

// 定义stop
void ThreadPool::stop()
{
  {
    std::unique_lock<std::mutex> lock(mutex_);
    running_ = false;
    cond_.notify_all();
  }
  for (std::thread &th : threads_)
  {
    th.join();
  }
}

// 添加任务
void ThreadPool::run(const Task &task)
{
  if (threads_.empty())
  {
    task();
  }
  else
  {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push_back(task);
    cond_.notify_one();
  }
}

ThreadPool::Task ThreadPool::take()
{

  std::unique_lock<std::mutex> lock(mutex_);
  cond_.wait(lock, [this] {
    return (!running_ || !queue_.empty());
  });
  Task task;
  if (!this->queue_.empty())
  {
    task = queue_.front();
    queue_.pop_front();
  }

  return task;
}

void ThreadPool::runInThread()
{
  try
  {
    while (running_ == true)
    {
      Task task(take());
      if (task)
      {
        task();
      }
    }
  }
  catch (...)
  {
    // deal with error
  }
}