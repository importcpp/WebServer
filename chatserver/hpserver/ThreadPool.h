#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
	ThreadPool(size_t);
	
	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)
		->std::future<typename std::result_of<F(Args...)>::type>;
	
	~ThreadPool();
private:
	// 用vector来管理线程池创建的线程 
	std::vector< std::thread > workers;
	// 利用队列管理加入的任务
	std::queue< std::function<void()> > tasks;

	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;
};

// 线程池初始化 -- 创建多线程，利用队列来管理任务
inline ThreadPool::ThreadPool(size_t threads)
	: stop(false)
{
	for (size_t i = 0; i < threads; ++i) {
		// lambda函数中的this是为了捕获类中的属性，通过thread的构造函数创建线程
		workers.emplace_back([this] {
				for (;;) // 利用for(;;)取代while(true)提高代码执行效率
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(this->queue_mutex);

						this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
						
						if (this->stop && this->tasks.empty()) return;
						
						// 获取队列最前面的任务开始执行，整个过程是原子操作
						task = std::move(this->tasks.front());
						this->tasks.pop();
						printf("Starting Jobs\n");
					}
					task();
				}
			}
		);
	}
}

// 
// 在线程池中加入任务
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
-> std::future<typename std::result_of<F(Args...)>::type>
{
	using return_type = typename std::result_of<F(Args...)>::type;

	auto task = std::make_shared< std::packaged_task<return_type()> >(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	std::future<return_type> res = task->get_future();
	{
		std::unique_lock<std::mutex> lock(queue_mutex);

		if (stop) throw std::runtime_error("enqueue on stopped ThreadPool");

		tasks.emplace([task]() { (*task)(); });
	}
	condition.notify_one();
	return res;
}

// 线程池销毁 -- 先退出目前正在进行的任务，然后等待所有线程完成任务
inline ThreadPool::~ThreadPool()
{
	{
		std::unique_lock<std::mutex> lock(queue_mutex);
		stop = true;
	}
	condition.notify_all();
	for (std::thread &worker : workers) {
		worker.join();
	}
}