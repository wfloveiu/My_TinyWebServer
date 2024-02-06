#include "http_conn.h"

#include <mysql/mysql.h>
#include <fstream>

//定义http响应的一些状态信息 
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";

locker m_lock;
map<string, string> users;
/*初始话静态变量*/
int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

void http_conn::init(int sockfd, const sockaddr_in &addr, char *root, int TRIGMod, int close_log, string user, string passwd, string sqlname)
{
    m_sockfd = sockfd;
    m_address = addr;

    doc_root = root;
    m_TRIGMode = TRIGMod;
    m_close_log = close_log;

    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    Utils tmp;
    tmp.addfd(m_epollfd, sockfd, true, m_TRIGMode);

    init();
}

void http_conn::init()
{
    mysql = NULL;
    byte_have_send = 0;
    byte_to_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_content_length = 0;
    m_start_line = 0;
    m_check_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;  
    timer_flag = 0;
    improv = 0;
    
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

bool http_conn::read_once()
{
    if(m_read_idx >= READ_BUFFER_SIZE)
        return false;
    int read_bytes = 0;
    // LT触发
    if(m_TRIGMode == 0)
    {
        read_bytes = recv(m_sockfd, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
        m_read_idx += read_bytes;

        if(read_bytes<=0)
            return false;
        return true;
    }
    //ET非阻塞模式
    else
    {
        while(1)
        {
            //疑问：m_read_idx下标可能会越界，如何解决
            read_bytes = recv(m_sockfd, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
            if(read_bytes == -1)
            {
                if(errno == EWOULDBLOCK || errno == EAGAIN) // socket非阻塞状态下，缓冲区中没有数据时，会返回这两种状态码
                    break;
                return false;
            }
            if(read_bytes == 0) //对端关闭
                return false;
            m_read_idx += read_bytes;
        }
        return true;
    }
}

void http_conn::process()
{

}

bool http_conn::write()
{
    
}