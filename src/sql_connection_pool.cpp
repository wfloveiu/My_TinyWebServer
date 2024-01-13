#include "sql_connection_pool.h"


connection_pool::connection_pool()
{
    m_CurConn = 0;
    m_FreeConn = 0;
}
connection_pool::~connection_pool()
{
    DestroyPool();
}
connection_pool * connection_pool::GetInstance()
{
    static connection_pool connPool;
    return &connPool;
}

void connection_pool::init(string Url, string User, string PassWord, string DatabaseName, int Port, int MaxConn, int close_log)
{
    m_Url = Url;
    m_User = User;
    m_PassWord = PassWord;
    m_DatabaseName = DatabaseName;
    m_Port = Port;
    m_MaxConn = MaxConn;

    for(int i = 1; i <= MaxConn; i++)
    {
        MYSQL * conn = NULL;
        conn = mysql_init(conn); //初始化MYSQL变量，返回句柄

        //初始化失败
        if(conn == NULL)
        {
            LOG_ERROR("MYSQL INIT ERROR");
            exit(1);
        }

        //数据库引擎建立连接
        conn = mysql_real_connect(conn, m_Url.c_str(), m_User.c_str(), m_PassWord.c_str(), m_DatabaseName.c_str(), m_Port, NULL, 0);

        if(conn == NULL)
        {
            LOG_ERROR("MYSQL CONNECTION BUILD ERROR");
            exit(1);
        }

        m_connList.push_back(conn);
        m_FreeConn++;
    }

    m_reserve = sem(m_FreeConn);
}

MYSQL * connection_pool::GetConnection()
{
    MYSQL * conn = NULL;

    if(m_connList.size() == 0)
        return NULL;
    //信号量减一
    m_reserve.wait();  //先wait，再去从list中获取连接

    m_lock.lock();

    conn = m_connList.front();
    m_connList.pop_front();

    m_FreeConn--;
    m_CurConn++;

    m_lock.unlock();

    return conn;
}
bool connection_pool::ReleaseConnection(MYSQL * conn)
{
    if(conn == NULL)
        return false;
    
    m_lock.lock();

    m_connList.push_back(conn);
    m_CurConn--;
    m_FreeConn++;

    m_lock.unlock();

    m_reserve.post(); //先放入list中，再post

    return true;
}

int connection_pool::GetFreeConnection()
{
    return this->m_FreeConn;
}

void connection_pool::DestroyPool()
{
    m_lock.lock();

    if(m_connList.size() > 0)
    {
        list<MYSQL *>::iterator it; //迭代器指针
        for(it = m_connList.begin(); it != m_connList.end(); it++)
        {
            MYSQL * conn = *it;
            mysql_close(conn);
        } 
        m_CurConn = 0;
        m_FreeConn = 0;
        m_connList.clear();
    }
    m_lock.unlock();
}

connectionRAII::connectionRAII(MYSQL ** conn, connection_pool * connpool)
{
    //传入的数据库连接句柄的指针和数据库连接池的指针
    //获取一个数据库连接
    *conn = connpool->GetConnection();
    //赋值给RAII的私有变量，在RAII对象作为局部变量时，退出作用域后，会自动调用这个类的析构函数，在
    //RAII的析构函数中释放这个数据库连接即可
    connRAII = *conn;
    pollRAII = connpool;
}

connectionRAII::~connectionRAII()
{
    pollRAII->ReleaseConnection(connRAII);
}