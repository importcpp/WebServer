#include "KThreadPool.h"
#include <chrono>
#include <iostream>

void print() {
  std::cout << "tid: " << std::this_thread::get_id() << std::endl;
}

void printString(const std::string &str) {
  std::cout << "tid: " << std::this_thread::get_id() << " str = " << str
            << std::endl;
  // printf("tid=%d, str=%s\n", muduo::CurrentThread::tid(), str.c_str());
}

int main() {
  ThreadPool pool("MainThreadPool");
  pool.start(5);

  pool.run(print);
  pool.run(print);
  for (int i = 0; i < 100; ++i) {
    pool.run(std::bind(printString, std::to_string(i)));
  }

  std::this_thread::sleep_for(std::chrono::seconds(3));
  pool.stop();

  return 0;
}