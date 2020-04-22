# Webserver

[![Build Status](https://travis-ci.org/linyacool/WebServer.svg?branch=master)](https://travis-ci.org/linyacool/WebServer) [![license](https://img.shields.io/github/license/mashape/apistatus.svg)](https://opensource.org/licenses/MIT)

## Introduction

本项目实现了一个基于Reactor模式的Web服务器, 支持Http长连接，可响应Get请求

## Environment

- OS: Ubuntu 16.04
- Complier: g++ 5.4.0
- Tools: CMake/VScode

## Technical points

* 基于Reactor模式构建网络服务器，编程风格偏向面向过程
* 采用**非阻塞IO**，IO复用模式默认是**ET**，可在编译前通过指定参数切换为LT模式，通过编译期确定工作模式，可以减少运行期不断判断条件造成的负担
* 线程间的工作模式: 主线程负责Accept请求，然后采用**Round-bin分发**的方式异步调用其他线程去管理请求端的IO事件
* 利用[AsyncWaker类](https://github.com/importcpp/WebServer/blob/master/webserver/loop/KAsyncWaker.h)(使用eventfd)实现线程间的异步唤醒
* **实现[LockFreeQueue](https://github.com/importcpp/WebServer/blob/master/webserver/lock/KLockFreeQueue.h)用于任务的异步添加与移除**，代替了常规的互斥锁管理临界区的方式 [这里的Lock free queue并没有解决ABA问题，但是针对这里单生产者单消费者模型，不会发生ABA问题]
* 实现[环形缓冲区](https://github.com/importcpp/WebServer/blob/master/webserver/tcp/KRingBuffer.h)作为Tcp读取数据和写入数据的缓冲类，使得数据被读取之后不需要移动其余元素的位置来在尾部腾出空间，针对环形缓冲区读或者写空间可能会出现不连续的情况，在Read和Write的处理上，使用了readv和writev系统调用读取不连续的内存(只需要一次系统调用)，解决了系统调用和拷贝带来的开销
* 采用智能指针管理对象的资源
* dev分支加入了Tcp Connection的**回收机制**，用于回收Tcp Connection对象中的资源，避免多次创建的开销
* ......

## Develop and Fix List

* 2019-12-19 Dev: 基本框架的实现
* 2020-03-04 Dev: 临界区的保护机制增加自旋锁，用于与互斥锁做性能对比
* 2020-03-14 Dev: 实现了Epoll ET模式 循环处理Accept、Read和Write事件
* 2020-03-26 Dev: 定义宏使得WebServer编译时确定Epoll的工作模式(ET/LT)！通过宏定义切换，方便压测对比实验
* 2020-04-04 **Important Dev:** 针对单生产者单消费者模型的特点，临界区的保护机制增加了Lockfree queue，用于与互斥锁做性能对比！目前的Lockfree queue，暂未解决CAS问题，后期会利用Hazard pointer解决。
* 2020-04-10 **Important Dev:** 针对muduo原本的Buffer类实现内部挪滕，增加数据拷贝开销的问题，实现了环形缓冲区类！设计的RingBuffer类的接口与Muduo原本的`vector<char>`接口保持一致，目前使用编译期宏定义的方式切换，方便之后做压测对比。之后会考虑设计一个KBuffer纯虚类，然后将RingBuffer和Muduo的Buffer作为作为KBuffer子类，利用C++多态，使用基类的指针指向子类的对象，这样来切换真正所使用的Buffer类.
* 2020-04-22 Dev: 针对每一个新来的连接都会创建一个Tcp Connection，连接断开后Tcp Connection的资源又会被全部回收的问题，采取了Tcp Connection的回收机制，使用vector存储Tcp Connection，再次新来连接时，只用修改Tcp Connection管理的连接对象，即可实现Tcp Connection的重用

## Todo list

**Update in 20-04-14**

* (Before ...) 添加tcp易产生粘包的解决方案
* (Before ...) 查查负载均衡模式，与round-bin作对比
* (Before ...) 异步唤醒机制采用管道 用于 与eventfd性能对比
* (Before ...) 多线程日志
* (Before ...) 实现定时器功能 (先小根堆试试，毕竟直接调库，然后写红黑树，可以参照Nginx，最后实现下淘宝Tengine中的四叉最小堆)



## Model Architecture

本服务器采用了事件循环 + 非阻塞IO的主题架构，通过事件驱动和事件回调来实现业务逻辑。事件循环用于做事件通知，如果有新的连接被Accept，则把新连接的socket对象的管理工作转交给其他线程。模型的整体架构如下图所示，模型的架构发展过程可见[History](https://github.com/importcpp/WebServer/blob/master/History.md).

<img src="https://github.com/importcpp/WebServer/raw/master/file/serverarch2.png" alt="v3" style="zoom: 80%;" />	

## Server Performance

* 使用Ringbuffer减少数据的拷贝拷贝次数，在一分钟内测试1000个并发连接，使用RingBuffer使得QPS提升10%

### Simple Comparison

压力测试使用了Webench，来自[linya](https://github.com/linyacool/WebServer)，谢谢！！！压力测试的方法也来自[linya](https://github.com/linyacool/WebServer/blob/master/测试及改进.md)，再次谢谢！！！

这里先做一个简要的对比，后期慢慢的详细对比，写一个更完整的文案

|                                            | QPS(响应消息体为短字符串) | QPS(响应消息体为长字符串) |
| :----------------------------------------: | :-----------------------: | :-----------------------: |
| [Muduo](https://github.com/chenshuo/muduo) |          115111           |            ---            |
|                    Mine                    |           94230           |            ---            |

这里只对长连接进行测试，线程模型是1个主线程+３个IO线程，为了对比线性Buffer和RingBuffer，使用了不同长度的响应消息体(长字符串对应5000个字符，短消息体对应12个字符)。下图是响应消息体为短字符串的测试结果(长字符还未对比)

* My Webserver 响应消息体为短字符串时测试结果

![mine_qps](https://github.com/importcpp/WebServer/raw/master/file/mine_qps.png)

* Muduo 响应消息体为短字符串时测试结果

![muduo_qps](https://github.com/importcpp/WebServer/raw/master/file/muduo_qps.png)



### Analysis

* 测试结果表明，相比muduo，我的服务器还是略逊的！
* 在搭建这个服务器的过程中，我的基本框架还有代码的风格(甚至一些直接的代码都是来自Muduo那本书)！但是呢，在慢慢搭建的过程中，我修改了Epoll的工作模式(LT->ET)，改进了缓冲区Buffer为环形缓冲区，还有线程间的异步任务调配我也为了避免锁的开销，改成了无锁结构，相比在这些做工作之前的服务器，我这个版本是有很大的性能提升的，这也是Muduo所没有的。
* 之所以结果比Muduo弱，我觉得更多的是一些细节上的问题，比如临时资源的创建等等！(总之还是先认真看一下Muduo源码，优化自己的服务器的细节吧！)

## How to improve

* 无锁队列任务调度是在主线程与IO线程中的其中一个线程之间进行的，也就是说这里是使用单生产者单消费者模型，其实可以改成单生产者多消费者模型，即主线程负责将任务enqueue到任务队列中，通知IO线程，然后空闲的IO线程们“主动”将任务从无锁任务队列中取出来，因为我这里还没有解决无锁队列容易产生的CAS问题(解决方法可以用引用计数或者Hazard Point)，所以暂时没有办法实现单生产者多消费者模型. 现成的优质的适用于单生产者多消费者模型的无锁队列有[concurrentqueue](https://github.com/cameron314/concurrentqueue)和 boost中的 [lockfreequeue]( https://www.boost.org/)

