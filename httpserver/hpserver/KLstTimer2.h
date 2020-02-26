#pragma once

#include <time.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/timerfd.h>
#include <iostream>
#include <functional>
#include "../utils/KTimestamp.h"

#define BUFFER_SIZE 64

extern const int TIMESLOT = 5;
extern const int NUMBER_TIME = 8;

namespace kb
{
int createTimerfd()
{
    // GQ 理解timerfd
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                   TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        // LOG_SYSFATAL << "Failed in timerfd_create";
        std::cout << "LOG_SYSFATAL:   "
                  << "Failed in timerfd_create" << std::endl;
        // abort();
    }
    return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
        microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    // LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
    //
    std::cout << "LOG_TRACE:   "
              << "TimerQueue::handleRead() " << n << " at " << now.toString() << std::endl;
    if (n != sizeof howmany)
    {
        // LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
        std::cout << "LOG_ERROR:   "
                  << "TimerQueue::handleRead() reads " << n << " bytes instead of 8" << std::endl;
    }
}

void resetTimerfd(int timerfd, Timestamp expiration)
{
    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    struct itimerspec oldValue;
    bzero(&newValue, sizeof newValue);
    bzero(&oldValue, sizeof oldValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    // 设置第一个为哨兵值
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
        // LOG_SYSERR << "timerfd_settime()";
        std::cout << "LOG_SYSERR:   "
                  << "timerfd_settime()" << std::endl;
    }
}
}; // namespace kb

//class util_timer; /*前向声明*/

// 定时器类
class util_timer
{
public:
    util_timer() : prev(nullptr), next(nullptr) {}

public:
    time_t expire;                 // 任务的超时时间，这里使用绝对时间
    std::function<void()> cb_func; // 任务回调函数，回调函数处理的客户数据，由定时器的执行者传递给回调函数

    util_timer *prev; // 指向前一个定时器
    util_timer *next; // 指向下一个定时器
};

// 定时器链表、 他是一个$$$升序$$$、双向链表，且带有头节点和尾节点
class sort_time_lst
{
public:
    sort_time_lst() : head(nullptr), tail(nullptr) {}

    // 链表被销毁时，删除其中所有的定时器
    ~sort_time_lst()
    {
        util_timer *tmp = head;
        while (tmp)
        {
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    // 添加节点 将目标定时器timer添加到链表中
    void add_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if (!head)
        {
            head = tail = timer;
            return;
        }
        // 如果目标定时器的超时时间小于当前链表中所有定时器的超时时间，则把该定时器插入链表头部，作为链表新的头节点，否则就调用重载函数add_timer
        if (timer->expire < head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    // 当某个定时器任务发生变化时，调整对应的定时器在链表中的位置。这个函数只考虑被调整的定时器的超时时间延长的情况，即该定时器需要往链表的尾部移动
    void adjust_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        util_timer *tmp = timer->next;
        if (!tmp || (timer->expire < tmp->expire))
        {
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer, head);
        }
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    // 将目标定时器timer从链表中删除
    void del_timer(util_timer *timer)
    {
        if (!timer)
        {
            return;
        }
        if ((timer == head) && (timer == tail))
        {
            delete timer;
            head = NULL;
            tail = NULL;
            return;
        }
        if (timer == head)
        {
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if (timer == tail)
        {
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    // SIGALRM 信号每次被触发就在其信号处理函数（如果使用统一事件源，则是主函数）中执行一次tick函数，以处理链表上到期的任务
    void tick()
    {
        if (!head)
        {
            return;
        }
        printf("timer tick\n");
        time_t cur = time(NULL);
        util_timer *tmp = head;
        while (tmp)
        {
            if (cur < tmp->expire)
            {
                break;
            }
            tmp->cb_func();
            head = tmp->next;
            if (head)
            {
                head->prev = NULL;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    // 一个重载的辅助函数，它被公有的add_timer函数和adjust_timer函数调用，该函数表示将目标定时器timer添加到节点lst_head之后的部分链表中
    void add_timer(util_timer *timer, util_timer *lst_head)
    {
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        // 遍历lst_head节点之后的部分链表，直到找到一个超时时间大于目标定时器的超时时间节点，并将目标定时器插入该节点中
        while (tmp)
        {
            if (timer->expire < tmp->expire)
            {
                prev->next = timer;
                timer->next = tmp;
                tmp->prev = timer;
                timer->prev = prev;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }

        // 特殊情况处理
        // 如果遍历完lst_head节点之后的部分链表，仍未找到超时时间大于目标定时器的超时时间的节点，则将目标定时器插入链表尾部，并把它设置为链表的新尾节点
        if (!tmp) // tmp == nullptr
        {
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            // 记录尾节点
            tail = timer;
        }
    }

private:
    util_timer *head;
    util_timer *tail;
};