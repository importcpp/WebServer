#include "KThreadPool.h"
#include <assert.h>


ThreadPool::ThreadPool(const std::string& name)
	:name_(name),
	running_(false)
{
}

ThreadPool::~ThreadPool() {
	if (running_) {
		stop();
	}
}

void ThreadPool::start(int numThreads)
{
	assert(threads_.empty());
	running_ = true;
	threads_.reserve(numThreads);

	for (int i = 0; i < numThreads; i++) {
		threads_.push_back(
			std::thread(std::bind(&ThreadPool::runInThread, this)));
	}

}

void ThreadPool::stop() 
{
	running_ = false;
	cond_.notify_all();
	for (std::thread &th : threads_) {
		th.join();
	}
}

void ThreadPool::run(const Task& task) {
	if (threads_.empty()) {
		task();
	}
	else {
		std::unique_lock<std::mutex> lock(mutex_);
		queue_.push_back(task);
		cond_.notify_one();
	}
}

ThreadPool::Task ThreadPool::take()
{
	std::unique_lock<std::mutex> lock(mutex_);

	
	this->cond_.wait(lock, [this] {
		return (!this->queue_.empty()) || (!this->running_);
	});

	Task task;
	if (!this->queue_.empty()) {
		task = queue_.front();
		this->queue_.pop_front();
	}
	return task;
}

void ThreadPool::runInThread() {
	
	try
	{
		while (running_)
		{
			Task task(this->take());
			// ??? 
			if (task) {
				task();
			}
		}
	}
	catch (const std::exception& ex)
	{
		std::cerr << "exception caught in ThreadPool " << name_ << std::endl;
		std::cerr << "reason: " << ex.what() << std::endl;
		std::abort();
	}
	catch (...) {
		std::cerr << "unknown exception caught in ThreadPool " << name_ << std::endl;
		abort();
	}
}
