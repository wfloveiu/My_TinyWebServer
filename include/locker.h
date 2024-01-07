/*
线程同步机制封装类
对信号量sem、条件变量、互斥锁进行封装，调用时更简洁
*/
#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <semaphore.h>
#include <stdexcept>

// 封装信号量
class sem
{
public:
    sem()
    {
        /*调用sem_init函数初始化m_sem信号量,初始化过程返回0
        第二个参数pshared表示允许几个进程共享这个信号量，为0表示用于进程内的多线程共享
        第三个参数value用于设置信号量的初始值*/
        if(sem_init(&m_sem,0,0) != 0)
            throw std::exception();
    }
    sem(int num)
    {
        if(sem_init(&m_sem,0,num) != 0)
            throw std::exception();
    }
    ~sem()
    {
        /*释放信号量*/
        sem_destroy(&m_sem);
    }
    bool wait()
    {
        //sem_wait成功返回0
        return sem_wait(&m_sem) == 0;
    }
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }
private:
    sem_t m_sem;  // 包含在semaphore.h中，声明信号量

};


// 封装互斥锁
class locker
{
public:
    locker()
    {
        /*
        初始化成功返回0
        第二个参数指定互斥锁的属性，NULL表示使用默认的互斥锁属性
        */
        if(pthread_mutex_init(&m_mutex, NULL) != 0)
            throw std::exception();
    }
    ~locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
private:
    pthread_mutex_t m_mutex;//定义一个锁
};

// 条件变量提供了一种线程间的通知机制,当某个共享数据达到某个值时,唤醒等待这个共享数据的线程.
class cond
{
public:
    cond()
    {
        // NULL表示是进程间使用
        if(pthread_cond_init(&m_cond, NULL) != 0)
            throw std::exception();
    }
    ~cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    // 等待条件变量,必须传一个已经加上锁的mutex过来
    bool wait(pthread_mutex_t *m_mutex)
    {
        int ret =  0;
        // 正确执行时返回值是0
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    //还要传一个等待时间
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t)
    {
        int ret =  0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal()  //条件变量满足条件，可以释放一个阻塞进程来使用它
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast()  //以广播的方式唤醒所有等待目标条件变量的线程
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }
private:
    pthread_cond_t m_cond;
};

#endif