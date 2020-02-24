#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>


class ThreadPool
{
public:
    typedef std::function<void()> Task;

    explicit ThreadPool(const std::string &name = std::string());
    ~ThreadPool();

    // 启动线程
    void start(int numThreads);
    // 终止线程
    void stop();

    // 添加任务
    void run(const Task &task);

private:
    //
    void runInThread();
    Task take();

    std::mutex mutex_;
    std::condition_variable cond_;
    std::vector<std::thread> threads_;
    std::deque<Task> queue_;
    bool running_;

    // 可以作为线程池标识
    std::string name_;
};
