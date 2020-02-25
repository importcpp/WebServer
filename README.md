# Http服务器

## Introduction

本项目实现了一个并发的http服务器

## Enviroment

- OS: Ubuntu 16.04
- Complier: g++ 5.4.0
- Tools: VScode 

## 技术要点

* 利用RAII方法封装了socket文件描述符对象，简化了TCP连接的处理
* IO复用: 使用epoll边沿触发，非阻塞IO
* 使用C++11中的线程库实现了线程池，利用队列管理任务
* 实现了基于升序链表的定时器管理非活动连接
* 实现了一个Buffer类处理应用层需要发送与接收的数据

