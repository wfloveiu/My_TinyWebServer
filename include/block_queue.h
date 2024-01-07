/*

*/
#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include "locker.h"
#include <iostream>
#include <queue>
using namespace std;

// 模板类
template <class T>
class block_queue
{
public:
    block_queue(int max_size = 1000)
    {
        if(max_size <= 0)
            exit(-1);
        m_max_size = max_size; 
    }
    bool full()
    {
        m_mutex.lock();
        if(m_queue.size() >= m_max_size)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    bool empty()
    {
        m_mutex.lock();
        if(m_queue.size() == 0)
        {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }
    // 获取队首元素
    bool get_front(T &value)
    {
        m_mutex.lock();
        if(m_queue.size() == 0)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_queue.front();
        m_mutex.unlock();
        return true;
    }
    // 获取队尾元素
    bool get_back(T &value)
    {
        m_mutex.lock();
        if(m_queue.size() == 0)
        {
            m_mutex.unlock();
            return false;
        }
        value = m_queue.back();
        m_mutex.unlock();
        return true;
    }
    // 获取队列的大小
    int get_size()
    {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_queue.size();
        m_mutex.unlock();
        return tmp;
    }
    // 添加元素
    bool push(const T &item)
    {
        m_mutex.lock();
        //阻塞队列满了的话，就唤醒阻塞的处理线程
        if(m_queue.size() >= m_max_size)
        {
            m_cond.broadcast();
            m_mutex.unlock();
            return false; 
        }
        m_queue.push(item);
        m_cond.broadcast();
        return true;
    }
    //
    bool pop()
    {
        
    }
private:
    int m_max_size; //阻塞队列的最大长度
    queue<T> m_queue; //队列是共享资源，使用与队列相关的函数时，要加锁
    locker m_mutex;
    cond m_cond; //条件变量
};

#endif