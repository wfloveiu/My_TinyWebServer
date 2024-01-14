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