#pragma once

#include <time.h>
#include <stdio.h>
#include <netinet/in.h>
#include <assert.h>
#include <functional>

class util_timer; // 前向声明

struct client_data
{
    int sockfd;
    util_timer *timer;
};

// 定时器类
class util_timer
{
public:
    util_timer() : prev(nullptr), next(nullptr) {}

public:
    time_t expire; // 任务的超时时间，这里使用绝对时间
    // 定时器管理的对象
    std::function<void(client_data *)> cb_func;
    struct client_data *user_data;
    util_timer *prev;
    util_timer *next;
};

// 定时器链表、 他是一个$$$升序$$$的双向链表
class sortTimeList
{
public:
    sortTimeList() : head(nullptr), tail(nullptr) {}
    ~sortTimeList()
    {
    }

    // 添加节点，将目标定时器timer添加到链表中
    void add_timer(util_timer *timer)
    {
        if (timer == nullptr)
        {
            return;
        }
        if (head == nullptr)
        {
            head = tail = timer;
            return;
        }
        // 可以直接插入到头部
        if (timer->expire <= head->expire)
        {
            timer->next = head;
            head->prev = timer;
            head = timer;
        }
        else
        {
            add_timer(timer, head->next);
        }
    }

    // 当某个定时器任务发生变化时，调整对应的定时器在链表中的位置。
    // 这个函数只考虑被调整的定时器的超时时间延长的情况，即该定时器需要往链表的尾部移动
    void adjust_timer(util_timer *timer)
    {
        if (timer == nullptr)
        {
            return;
        }
        if (timer->next == nullptr || (timer->expire <= timer->next->expire))
        {
            return;
        }

        if (timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            add_timer(timer, head);
        }
        else
        {
            timer->prev->next = timer->next;
            timer->next->prev = timer->prev;
            add_timer(timer, timer->next);
        }
    }

    // 将目标定时器timer从链表中移除
    void del_timer(util_timer *timer) {
        if(timer == nullptr)
        {
            return;
        }
        if((timer == head) && (timer == tail))
        {
            delete timer;
            head = tail = nullptr;
            return;
        }
        if(timer == head)
        {
            head = head->next;
            head->prev = nullptr;
            delete timer;
            return;
        }
        if(timer == tail)
        {
            tail = tail->prev;
            tail->next = nullptr;
            delete timer;
            return;
        }

    }

    // SIGALRM 信号每次被触发就在其信号处理函数（如果使用统一事件源，则是主函数）中
    // 执行一次tick函数，以处理链表上到期的任务
    void tick()
    {
        if (head == nullptr)
        {
            return;
        }
        printf("timer tick\n");
        time_t cur = time(nullptr);
        util_timer *tmp = head;
        while (tmp != nullptr)
        {
            if (cur < tmp->expire)
            {
                break;
            }
            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if (head != nullptr)
            {
                head->prev = nullptr;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    // 一个重载的辅助函数，它被公有的add_timer函数和adjust_timer函数调用，
    // 该函数表示将目标定时器timer添加到节点watchdog之后的部分链表中
    void add_timer(util_timer *timer, util_timer *watchdog)
    {
        assert(watchdog != head);
        while (watchdog != nullptr)
        {
            if (timer->expire <= watchdog->expire)
            {
                ///
                watchdog->prev->next = timer;
                timer->prev = watchdog->prev;
                timer->next = watchdog;
                watchdog->prev = timer;
                return;
            }
            watchdog = watchdog->next;
        }

        // 特殊 - 插入尾部即可
        if (watchdog == nullptr)
        {
            tail->next = timer;
            timer->prev = tail;
            timer->next = nullptr;
            tail = timer;
        }
    }

private:
    util_timer *head;
    util_timer *tail;
};