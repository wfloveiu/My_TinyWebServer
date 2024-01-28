#include "./config/config.h"



int main(int argc, char * argv[])
{

    string user = "";
    string password = "";
    string databasename = "";

    Config config;//默认构造参数
    config.parse_arg(argc, argv); //按照命令行中传入的参数更改

    Websever server;
    //初始化server
    server.init(config.PORT, user, password, databasename, config.LOGWrite,
                config.OPT_LINGER, config.TRIGMod, config.sql_num, config.thread_num,
                config.close_log, config.actor_model);
    //初始化日志
    server.log_write();

    //数据库连接池
    server.sql_pool();

    //触发模式
    server.TRIGMod();

    //线程池
    server.thread_pool();
    
    //监听
    server.eventListen();

    //运行
    server.eventloop();

}