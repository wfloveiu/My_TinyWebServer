#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <bits/getopt_core.h>
#include <string>
#include "../log/log.h"
#include "../sql_connection_pool/sql_connection_pool.h"
#include "../thredpool/threadpool.h"
#include <sys/socket.h>
#include <cassert>
#include <netinet/in.h>
#include "../lst_timer/lst_timer.h"
#include "../http_conn/http_conn.h"

using namespace std;

const int MAX_FD = 65536; //最大的连接数
const int MAX_EVENT_NUMBER = 10000; //最大的事件数量
const int TIMESLOT = 5; //每5s产生一次alarm信号
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

    //初始化线程池
    void thread_pool();

    //触发模式
    void TRIGMod();

    //设置监听相关
    void eventListen();

    //运行
    void eventloop();
private:
    bool deal_clentdata();
public:
    // 服务器运行的端口号
    int m_port;
    //写日志的方式，0同步，1异步
    int m_log_write;
    //优雅关闭连接，0不使用，1使用
    int m_opt_linger;
    int m_trigmod;
    //listenfd模式
    int m_LISTENTrigmode;
    //connfd模式
    int m_CONNTrigmode;
    //日志是否关闭，即是否使用日志
    int m_close_log;
    int m_actor_model;
 
    /*数据库相关*/
    connection_pool * m_connection_pool;
    //数据库服务器的运行地址
    string m_sqlUrl;
    int m_sqlPort;
    string m_user;
    string m_password;
    string m_databasename;
    //数据库连接池中的最大连接数
    int m_sql_num;

    /*线程池相关*/ 
    threadpool<http_conn> * m_threadpool;
    //线程池中的最大连接数
    int m_thread_num;

    //定时器相关
    client_data *user_timer;
    Utils utils;

    //监听连接的套接字
    int m_listenfd;
    //epoll文件描述符
    int m_epollfd;
    //utils中主进程与信号接受进程之间通信
    int m_pipefd[2];
    http_conn *users;
    char * m_root;
};


#endif