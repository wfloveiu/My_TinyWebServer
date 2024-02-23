#ifndef LOG_H
#define LOG_H

#include "block_queue.h"
#include "../lock/locker.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <sys/time.h>
#include <cstdarg>
using namespace std;
/*
1、私有化构造函数，防止外界创建单例类的对象
2、使用类的私有静态指针变量指向类的唯一实例
3、使用公有的静态方法获取该实例
使用的是懒汉模式
*/
class Log
{
public:
    static Log * get_instance()
    {
        // 使用静态局部变量作为单例对象
        static Log instance;
        return &instance;
    } 
    static void * flush_log_thread(void  *args)
    {
        Log::get_instance()->async_write_log();
    }
    // 参数为：日志文件路径、是否关闭、日志缓冲区大小、每个日志文件的最大行数
    bool init(const char * file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

    void write_log(int level, const char* format, ...);

    void flush();
private:
    Log(); //私有化构造函数
    virtual ~Log();
    void * async_write_log()
    {
        string single_log;
        // 获取阻塞队列这个对象，并取出日志
        while(m_log_queue->pop(single_log))
        {
            m_mutex.lock();
            // 写入日志文件
            fputs(single_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }
private: 
    char log_name[128];  //日志文件的名字
    char log_dirname[128]; //日志文件的路径
    long long m_count;  // 日志今天写了多少行，每到新一天，重置
    int m_log_buf_size;  //日志缓冲区大小
    char *m_buf;   //日志缓冲区
    int m_close_log; //关闭日志，
    int m_split_lines;  //每个日志文件最多可以有多少行日志
    bool m_is_async;            //是否同步标志位
    int m_today;
    FILE *m_fp;                 //打开的log文件的文件指针
    block_queue<string> * m_log_queue;  // 阻塞队列
    locker m_mutex;
};

#define LOG_DEBUG(format, ...) if(0==m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0==m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0==m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0==m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#endif