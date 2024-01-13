#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <bits/getopt_core.h>
#include <string>
#include "log.h"

using namespace std;

const int MAX_FD = 65536; //最大的连接数
const int MAX_EVENT_NUMBER = 10000; //最大的事件数量

class Websever
{
public:
    Websever();
    ~Websever();
    void init(int port, string user, string password, string databasename, int log_write,
              int opt_linger, int trigmod, int sql_num, int thread_num, int close_log, int actor_model);
    
    // 初始化日志类
    void log_write();

    //初始化数据库连接池
    void sql_pool();
private:

public:
    int m_port;
    //写日志的方式，0同步，1异步
    int m_log_write;
    int m_opt_linger;
    int m_trigmod;
    //日志是否关闭，即是否使用日志
    int m_close_log;
    int m_actor_model;

    //数据库相关
    string m_user;
    string m_password;
    string m_databasename;
    int m_sql_num;

    // 线程池相关
    int m_thread_num;
};


#endif