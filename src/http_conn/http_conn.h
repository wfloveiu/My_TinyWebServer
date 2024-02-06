#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include "../log/log.h"
#include "../lock/locker.h"
#include "../lst_timer/lst_timer.h"
#include <map>

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    //表示一共有多少个http_conn连接
    static int m_user_count;
    //内核注册表文件按描述符
    static int m_epollfd;
    //这个http连接使用的数据库连接
    MYSQL *mysql;
   
    int m_state;   //reactor下在任务线程中对于要处理任务的标识，0表示处理读操作，1表示处理写操作
    int timer_flag; //读取数据失败时，将次位设置为1，从而在主线程中删除该连接所有相关
    int improv;
    enum METHOD  //枚举类型，请求方法
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // 主状态机的状态，标识解析位置。
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0, //当前正在分析请求行
        CHECK_STATE_HEADER, // 当前正在分析请求头
        CHECK_STATE_CONTENT // 当前正在分析请求体
    };
    //报文解析结果
    enum HTTP_CODE // 定义的是枚举类型，这个类型中有以下这么多的值
    {
        NO_REQUEST, // 报文处理还没结束，需要继续处理请求报文数据
        GET_REQUEST,// 获得了完整的HTTP请求
        BAD_REQUEST, // HTTP请求报文有语法错误
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // 从状态机的状态，标识解析一行的读取状态。
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    }; 

public:
    http_conn();
    ~http_conn();

    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);

    bool read_once();

    void process();
    //响应写回缓冲区
    bool write();

    sockaddr_in * get_address()
    {
        return & m_address;
    }
private:
    void init();
private:
    //http连接的套机口
    int m_sockfd; 
    //客户端地址
    sockaddr_in m_address;
    int m_TRIGMode;
    char *doc_root; //资源文件夹所在目录
    int m_close_log;

    /*数据库连接相关*/
    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];

    /*读取请求报文相关变量*/
    char m_read_buf[READ_BUFFER_SIZE];  //读缓冲区
    long m_read_idx;    //读缓冲区中第一个未被填充字符的下标
    

    /*解析请求报文相关变量*/
    CHECK_STATE m_check_state;
    METHOD m_method;    //请求方法
    long m_check_idx;   //读缓冲区中解析到哪个位置
    long m_start_line;
    /*请求报文中的6个变量*/
    char m_real_file[FILENAME_LEN];
    char *m_url;
    char *m_version;
    char *m_host;
    long m_content_length;
    bool m_linger;     //浏览器连接状态，true为保持连接

    //暂时未定义
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;
    char *m_string;

    /*发送报文相关变量*/
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx; 
    int byte_to_send; //还需发送的字节
    int byte_have_send; //已经发送的字节

};


#endif