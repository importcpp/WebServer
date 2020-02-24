#include "KThreadPool.h"
#include <iostream>
#include <chrono>

using namespace kb;

void print()
{
    std::cout << "tid: " << std::this_thread::get_id()
              << std::endl;
}

void printString(const std::string &str)
{
    std::cout << "tid: " << std::this_thread::get_id()
              << " str = " << str << std::endl;
}

int main()
{
    ThreadPool pool("Test");
    pool.start(5);

    pool.run(print);
    pool.run(print);

    for (int i = 0; i < 100; ++i)
    {
        pool.run(std::bind(printString, std::to_string(i)));
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    pool.stop();

    return 0;
}