# 基于QT的多人聊天服务器

[![Build Status](https://camo.githubusercontent.com/2082daf7628cf1739d9542b7c119d3843978add4/68747470733a2f2f7472617669732d63692e6f72672f6c696e7961636f6f6c2f5765625365727665722e7376673f6272616e63683d6d6173746572)](https://travis-ci.org/linyacool/WebServer) [![license](https://camo.githubusercontent.com/b0224997019dec4e51d692c722ea9bee2818c837/68747470733a2f2f696d672e736869656c64732e696f2f6769746875622f6c6963656e73652f6d6173686170652f6170697374617475732e737667)](https://opensource.org/licenses/MIT)

## Introduction

本项目主要使用了C++11编写了一个简易的聊天室服务器，实现了多用户同时在线实时聊天的功能

## Enviroment

- OS: Ubuntu 16.04
- Complier: g++ 5.4.0
- Tools: VScode and Qt

## 技术要点

* 数据的异步收发使用了TCP协议
* IO复用: 使用epoll边沿触发，非阻塞IO
* 线程池: （参照了[此处](https://github.com/progschj/ThreadPool)）使用C++11实现，利用队列管理线程
* 使用基于升序链表的定时器管理非活动连接，后续会更新为时间轮或者时间堆
* 客户端qt: 利用独立的线程管理数据接收，采用qt的信号与槽的机制管理数据发送

## Build

``````bash
g++ -I. -g -std=c++11 chatserver.cpp -o server.out -lpthread
``````

## Usage

``````bash
./server.out 127.0.0.1 9090
``````

> 客户端主要利用Qt编写，使用Qt加载项目打开即可，其主要运行界面如下图所示：

![ui](./pic/qtui.png)

