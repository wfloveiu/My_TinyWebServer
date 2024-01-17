#include "webserver.h"




//默认构造
Websever::Websever()
{
    m_sqlUrl = "localhost";
    m_sqlPort = 3306;
}

Websever::~Websever()
{

}

void Websever::init(int port, string user, string password, string databasename, int log_write,
              int opt_linger, int trigmod, int sql_num, int thread_num, int close_log, int actor_model)
{
    m_port = port;
    m_user = user;
    m_password = password;
    m_databasename = databasename;
    m_log_write = log_write;
    m_opt_linger = opt_linger;
    m_trigmod = trigmod;
    m_sql_num = sql_num;
    m_thread_num = thread_num;
    m_close_log = close_log;
    m_actor_model = actor_model;
}

void Websever::log_write()
{
    //如果是打开日志
    if(!m_close_log)
    {
        // 异步写日志
        if(m_log_write == 1)
        //单实例类，需要先获取这个单实例对象
            Log::get_instance()->init("./ServerLog",m_close_log, 2000, 800000, 800);
        else
            Log::get_instance()->init("./ServerLog",m_close_log, 2000, 800000, 0);
    }
}

void Websever::TRIGMod()
{
    if(m_trigmod == 0)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 0;

    }
    else if(m_trigmod == 1)
    {
        m_LISTENTrigmode = 0;
        m_CONNTrigmode = 1;
    }
    else if(m_trigmod == 2)
    {
        m_LISTENTrigmode == 1;
        m_CONNTrigmode = 0;
    }
    else if(m_trigmod == 3)
    {
        m_LISTENTrigmode = 1;
        m_CONNTrigmode = 1;
    }
}

void Websever::sql_pool()
{
    //类的static函数只能通过类名加::来调用
    m_connection_pool = connection_pool::GetInstance();

    // 初始化数据库连接池
    m_connection_pool->init(m_sqlUrl, m_user, m_password, m_databasename, m_sqlPort, m_sql_num, m_close_log);

    //初始化数据库读取表
}

void Websever::thread_pool()
{
    m_threadpool = new threadpool<http_conn>(m_actor_model, m_connection_pool, m_thread_num);
}

void Websever::eventListen()
{
    //创建监听套接字，成功返回>=0
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);//使用assert函数检验文件描述符是否>=0，<0就打印错误并终止程序

    //设置连接关闭的方式
    if(0 == m_opt_linger)
    {
        struct linger tmp = {0, 1}; 
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(1 == m_opt_linger)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    //创建ipv4监听地址
    struct sockaddr_in address; //需要<netinet/in.h>
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    // 设置SO_REUSEADDR可以强制使用处于TIME_WAIT状态的连接占用的socket,在重启服务器时能马上重新绑上原来的端口
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    //绑定IP地址和监听套接字
    int ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    //开始监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);


}