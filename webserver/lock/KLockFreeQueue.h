#pragma once
#include "../utils/Knoncopyable.h"
#include <memory>
#include <atomic>
#include <vector>

template <typename T>
class LockFreeQueue
{
    // 用链表实现LockFreeQueue --- Node定义节点的类型
private:
    struct Node
    {
        T data_;
        Node *next_;
        Node() : data_(), next_(nullptr) {}
        Node(T &data) : data_(data), next_(nullptr) {}
        Node(const T &data) : data_(data), next_(nullptr) {}
    };

private:
    Node *head_, *tail_;

public:
    LockFreeQueue() : head_(new Node()), tail_(head_) {}
    ~LockFreeQueue()
    {
        // 添加析构
    }

    // 代码逻辑来自:
    // https://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf
    bool Try_Dequeue(T &data)
    {
        Node *old_head, *old_tail, *first_node;
        for (;;)
        {
            // 取出头指针，尾指针，和第一个元素的指针
            old_head = head_;
            old_tail = tail_;
            first_node = old_head->next_;

            // head_ 指针已移动，重新取 head指针
            if (old_head != head_)
            {
                continue;
            }

            // 判断是否是空队列，并且
            if (old_head == old_tail)
            {
                if (first_node == nullptr)
                {
                }
                ::__sync_bool_compare_and_swap(&tail_, old_tail, first_node);
                continue;
            }

            //移动 old_head 指针成功后，取出数据
            if (::__sync_bool_compare_and_swap(&head_, old_head, first_node) == true)
            {
                data = first_node->data_;
                break;
            }
        }
        delete old_head;
        return true;
    }

    // Todo: 一次性将当前任务取出来，减少调用
    bool Try_Dequeue_Bunch()
    {
        return false;
    }

    void Enqueue(const T &data)
    {
        Node *enqueue_node = new Node(data);
        Node *copy_tail, *copy_tail_next;

        for (;;)
        {
            //先取一下尾指针和尾指针的next
            copy_tail = tail_;
            copy_tail_next = copy_tail->next_;

            //如果尾指针已经被移动了，则重新开始
            // 通过不断循环确保 copy_tail一定是当前所有线程中的尾指针
            if (copy_tail != tail_)
            {
                continue;
            }

            //如果尾指针的 next 不为NULL，则 fetch 全局尾指针到next
            if (copy_tail_next != nullptr)
            {
                ::__sync_bool_compare_and_swap(&(tail_), copy_tail, copy_tail_next);
                continue;
            }

            //如果加入结点成功，则退出
            if (::__sync_bool_compare_and_swap(&(copy_tail->next_), copy_tail_next, enqueue_node) == true)
            {
                break;
            }
        }
        // 重置尾节点, (也有可能已经被别的线程重置
        ::__sync_bool_compare_and_swap(&tail_, copy_tail, enqueue_node);
    }
};
