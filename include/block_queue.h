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
    bool push(const T &item)
    {
        m_mutex.lock();
        //阻塞队列满了的话，就唤醒阻塞的处理线程
        while(m_queue.size() >= m_max_size)
        {
            m_cond_canpop.broadcast();
            m_cond_canpush.wait(m_mutex.get());
 
        }
        m_queue.push(item);
        m_cond_canpop.broadcast();
        return true;
    }
    
    bool pop(T &item)
    {
        m_mutex.lock();
        while(m_queue.size()<=0)
        {
            if(!m_cond_canpop.wait(m_mutex.get()))//获得锁的指针
            {
                m_mutex.unlock();
                return false;
            }
        }
        item = m_queue.front();
        m_queue.pop();// 将队首弹出
        m_mutex.unlock();
        m_cond_canpush.broadcast();
        return true;
    }
private:
    int m_max_size; //阻塞队列的最大长度
    queue<T> m_queue; //队列是共享资源，使用与队列相关的函数时，要加锁
    locker m_mutex;
    cond m_cond_canpop;  //条件变量,可以取队首元素
    cond m_cond_canpush; //可以push到队尾
};

#endif