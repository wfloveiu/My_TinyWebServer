#include "webserver.h"



//默认构造
Websever::Websever()
{

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

void Websever::sql_pool()
{
    
}