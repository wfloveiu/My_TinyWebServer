/*主线程往任务队列中插入任务，工作线程竞争地从任务队列中取出任务，去执行*/
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "../sql_connection_pool/sql_connection_pool.h"

template <typename T>
class threadpool
{
public:
/*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int actor_model, connection_pool * connpool, int thread_num = 8, int max_request = 10000);
    ~threadpool();
    //添加请求
    bool append(T * request, int staete);

    bool append_p(T * request);
private:
    void * worker(void *arg);
    //真正的执行请求的函数
    void run();
private:
    pthread_t * m_threads;
    //工作线程数量
    int m_thread_num;
    //请求队列中最多允许的请求数量
    int m_max_request;
    //模型
    int m_actor_model;
    //请求队列
    std::list<T*> m_queue;
    //锁，保证对请求队列的互斥访问
    locker m_queuelock;
    //信号量，控制生产者和消费者的同步，也可以使用cond条件变量
    sem m_sem;
    connection_pool * m_connection_pool;
};


template <typename T>
threadpool<T>::threadpool(int actor_model, connection_pool * connpool, int thread_num, int max_request):
m_actor_model(actor_model), m_connection_pool(connpool), m_thread_num(thread_num), m_max_request(max_request), m_threads(NULL)
{
    if(thread_num<=0 || max_request<=0)
        throw std::exception();
    // 在内存中开辟thread_num个连续的内存空间，并把起始地址给m_pthread
    m_threads = new pthread_t[thread_num];
    if(!m_threads)
        throw std::exception();
    //初始化线程，并设置为detach
    for(int i=0; i<thread_num; i++)
    {
        //指针的算术运算是以指针指向的数据类型的大小为单位的
        //成功返回0
        //pthread_create必须是静态函数，参考：https://www.cnblogs.com/shijingxiang/articles/5389294.html
        if(pthread_create(m_threads+i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        //将线程分离，主线程不用等待,成功返回0
        if(pthread_detach(m_threads[i]) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
//new的东西要手动释放
threadpool<T>::~threadpool()
{
    delete[] m_threads;//释放m_threads开始的连续内存空间
}


template <typename T>
bool threadpool<T>::append(T * request, int state)
{
    m_queuelock.lock();
    if(m_queue.size() >= m_max_request)
    {
        m_queuelock.unlock();
        return false;
    }
    request->state = state;
    m_queue.push_back(request);
    m_queuelock.unlock();

    // 通过信号量提醒有任务要处理
    m_sem.post();
    return true;
}


template <typename T>
bool threadpool<T>::append_p(T * request)
{
    m_queuelock.lock();
    if(m_queue.size() >= m_max_request)
    {
        m_queuelock.unlock();
        return false;
    }
    // request->state = state;
    /*对于proactor，在任务线程中，不需要区分读还是写任务，因为读缓冲区和写缓冲区都是在夫进程中完成的*/
    m_queue.push_back(request);
    m_queuelock.unlock();

    // 通过信号量提醒有任务要处理
    m_sem.post();
    return true;
}


template <typename T>
void * threadpool<T>::worker(void * arg)
{
    //类型转换，arg是void类型，需要转换成threadpool类型 
    threadpool * pool = (threadpool*)arg;
    //只能通过对象指针的形式调用非静态成员函数
    pool->run();
    return pool;
}

template <typename T>
void threadpool<T>::run()
{
    while(true)
    {
        m_sem.wait(); //信号量等待，非零说明队列中有任务要处理
        m_queuelock.lock();
        if(m_queue.empty())
        {
            m_queuelock.unlock();
            continue;
        }
        T * request = m_queue.front();
        m_queue.pop_front();
        m_queuelock.unlock();

        if(!request)
            continue;

        //reactor模式
        if(m_actor_model == 1)
        {
            if(request->m_state == 0)
            {
                if(request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connection_pool); //为连接申请数据库连接
                    request->process(); //处理请求
                }
                不知道为什么;
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else
            {
                if(request->())
                    request->improv = 1;
                else
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else
        {
            connectionRAII mysqlcon(&request->mysql, m_connection_pool); //为连接申请数据库连接
            request->process(); //处理请求
        }
    }
}
#endif