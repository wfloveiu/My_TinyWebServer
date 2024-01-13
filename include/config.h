#ifndef CONFIG_H
#define CONFIG_H


#include "webserver.h"
using namespace std;

class Config
{
public:
    Config();
    ~Config();
    void parse_arg(int argc, char * argv[]);
public:
    // 服务器监听的端口号
    int PORT;

    // 日志写入方式:0同步写入，1异步写入
    int LOGWrite;

    // 触发组合模式:默认是0+0
    int TRIGMod;

    //listenfd触发模式:0是LT，1是ET
    int LISTENTrigmode;

    //connfd触发模式:0是LT，1是ET
    int CONNTrigmode;

    //优雅关闭连接:0不使用，1使用
    int OPT_LINGER;

    //数据库连接池数量
    int sql_num;

    //线程池内的线程数量
    int thread_num;

    //是否关闭日志:0打开，1关闭
    int close_log;

    //并发模型选择:0是Proactor模型，1是Reactor
    int actor_model;
};

#endif