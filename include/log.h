#ifndef LOG_H
#define LOG_H

#include "block_queue.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
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
    // 参数为：日志文件路径、是否关闭、日志缓冲区大小、每个日志文件的最大行数
    bool init(const char * file_name, int close_log, int log_buf_size = 8192, int split_lines = 5000000, int max_queue_size = 0);

private:
    Log(); //私有化构造函数
    virtual ~Log();
private:
    char *log_name;  //日志文件的名字
    char *log_dirname; //日志文件的路径
    long long m_present_count;  // 日志目前的行数
    int m_log_buf_size;  //日志缓冲区大小
    char *m_buf;   //日志缓冲区
    int m_close_log; //关闭日志，
    int m_split_lines;  //每个日志文件最多可以有多少行日志
    bool m_is_async;            //是否同步标志位
    int m_today;
    FILE *m_fp;                 //打开的log文件的文件指针
    block_queue<string> * m_log_queue;  // 阻塞队列

};











#endif