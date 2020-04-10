# Webserver

## Introduction

本项目实现了一个基于Reactor模式的Web服务器, 支持Http长连接，可响应Get请求

## Enviroment

- OS: Ubuntu 16.04
- Complier: g++ 5.4.0
- Tools: CMake/VScode

## Technical points

* 基于Reactor模式构建网络服务器，编程风格偏向面向过程
* 采用**非阻塞IO**，IO复用模式默认是**ET**，可在编译前通过指定参数切换为LT模式，通过编译期确定工作模式，可以减少运行期不断判断条件造成的负担
* 线程间的工作模式: 主线程负责Accept请求，然后采用**Round-bin分发**的方式异步调用其他线程去管理请求端的IO事件
* 利用**AsyncWaker类**(使用eventfd)实现线程间的异步唤醒
* **实现LockFreeQueue用于任务的异步添加与移除**，代替了常规的互斥锁管理临界区的方式 [这里的Lock free queue并没有解决ABA问题，但是针对这里单生产者单消费者模型，不会发生ABA问题]
* 实现**环形缓冲区**作为Tcp读取数据和写入数据的缓冲类，使得数据被读取之后不需要移动其余元素的位置来在尾部腾出空间
* 采用智能指针管理对象的资源
* ......

## Develop and Fix List

* 2019-12-19 Dev: 基本框架的实现
* 2020-03-04 Dev: 临界区的保护机制增加自旋锁，用于与互斥锁做性能对比
* 2020-03-14 Dev: 实现了Epoll ET模式 循环处理Accept、Read和Write事件
* 2020-03-26 Dev: 定义宏使得WebServer编译时确定Epoll的工作模式(ET/LT)
* 2020-04-04 **Important Dev:** 针对单生产者单消费者模型的特点，临界区的保护机制增加了Lockfree queue，用于与互斥锁做性能对比
* 2020-04-10 **Important Dev:** 针对muduo原本的Buffer类实现内部挪滕，增加数据拷贝开销的问题，实现了环形缓冲区类！

## Todo list

**Update in 20-04-07**

* (Before 4.16) 实现定时器功能 (先小根堆试试，毕竟直接调库，然后写红黑树，可以参照Nginx，最后实现下淘宝Tengine中的四叉最小堆)
* (Before ***) 压力测试(LT模式与ET模式对比，异步唤醒临界区争用性能对比)，补补压力测试理论知识
* (Before 4.23) 添加tcp易产生粘包的解决方案
* (Before 4.27) 查查负载均衡模式，与round-bin作对比
* (Before 5.3) 异步唤醒机制采用管道 用于 与eventfd性能对比
* (Before ...) 多线程日志

## Model Architecture

本服务器采用了事件循环 + 非阻塞IO的主题架构，通过事件驱动和事件回调来实现业务逻辑。事件循环用于做事件通知，如果有新的连接被Accept，则把新连接的socket对象的管理工作转交给其他线程。模型的整体架构如下图所示，模型的架构发展过程可见[History]().

<img src="https://github.com/importcpp/httpServer/raw/master/file/serverarch2.png" alt="v3" style="zoom: 80%;" />