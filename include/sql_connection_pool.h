#ifndef _SQL_CONNECTION_POOL
#define  _SQL_CONNECTION_POOL

#include <stdio.h>
#include <string.h>
#include <string>
#include <mysql/mysql.h>
#include <list>
#include "log.h"
#include "locker.h"
using namespace std;

// 单例模式
class connection_pool
{
public:
    static connection_pool * GetInstance();
    void init(string Url, string User, string PassWord, string DatabaseName, int Port, int MaxConn, int close_log);
    // 获取一个数据库连接
    MYSQL * GetConnection();
    // 释放当前的数据库连接
    bool ReleaseConnection(MYSQL * conn);
    // 获取当前的空闲连接数
    int GetFreeConnection();
    // 销毁数据路连接池
    void DestroyPool();
private:
    connection_pool();
    ~connection_pool();

    int m_MaxConn; //最大连接数
    int m_CurConn; //当前已使用的连接数
    int m_FreeConn;//空闲的连接数

    locker m_lock;
    list<MYSQL *> m_connList;
    sem m_reserve; //信号量，表示连接池中可用的数据库连接这个共享资源
public:
    string m_Url; //数据库服务器ip地址
    int m_Port;
    string m_User; //登录的用户名
    string m_PassWord;
    string m_DatabaseName; //数据库名
    int m_close_log; //日志开关,用处？
};

class connectionRAII
{
public:
    connectionRAII(MYSQL ** conn, connection_pool * connpool);
    ~connectionRAII();
private:
    MYSQL * connRAII;
    connection_pool * pollRAII;
};

#endif