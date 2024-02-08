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

//从状态机，用于分析出一行内容
//在HTTP报文中，每一行的数据由\r\n作为结束字符，空行则是仅仅是字符\r\n。因此，可以通过查找\r\n将报文拆解成单独的行进行解析
http_conn::LINE_STATUS http_conn::parse_line()
{
    char tmp;
    for(;m_check_idx < m_read_idx; m_check_idx++)
    {
        tmp = m_read_buf[m_check_idx];
        if(tmp == '\r')
        {
            //下个位置是read_idx说明数据接受不完整
            if(m_check_idx + 1 == m_read_idx)
                return LINE_OPEN; 
            //下个字符是\n,则将这两个位置的字符都改成\0
            else if(m_read_buf[m_check_idx+1] == '\n')
            {
                m_read_buf[m_check_idx++] = '\0';
                m_read_buf[m_check_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;  //正常的数据部分时不能有'\r'的
        }
    }
    return LINE_OPEN;
}

http_conn::HTTP_CODE http_conn::analyze_request_line(char * text)
{
    // http请求行的格式为<method> <URL> <version>各部分以空格或者制表符"\t"分隔
    m_url = strpbrk(text, " \t"); //比较字符串str1和str2中是否有相同的字符，如果有，则返回该字符在str1中的位置的指针。
    if(!m_url)
        return BAD_REQUEST;
    *m_url++ = '\0';  //字符串分割
    m_url += strspn(m_url," \t"); //检索字符串 str1 中第一个不在字符串 str2 中出现的字符下标。可能出现连续的空格或者制表符

    char * method = text;
    if(strcasecmp(text,"GET") == 0)
        m_method = GET;
    else if(strcasecmp(text,"POST") == 0)
        m_method = POST,cgi = 1;
    else
        return BAD_REQUEST; //仅仅支持get和post

    
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version+=strspn(m_version, " \t");
    /*http://IP地址:端口号/.....*/
    if(strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url,'/'); //忽略地址和端口号，定位到资源所在文件位置
    }
    else if(strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url,'/'); 
    }
    if(!m_url)
        return BAD_REQUEST;
    /*
    如果端口号后只有'/'，未定位到具体文件，则默认是judge.html
    */
    if(strlen(m_url) == 1)
        strcat(m_url, "judge.html");
    
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::analyze_request_header(char * text)
{
    /*
    请求头字符格式如下：
    Content-Length: 55\r\n
    Content-Type: text/html\r\n
    Last-Modified: Wed, 12 Aug 1998 15:03:50 GMT\r\n
    Accept-Ranges: bytes\r\n
    ETag: “04f97692cbd1:377”\r\n
    Date: Thu, 19 Jun 2008 19:29:07 GMT\r\n
    每一对key-value需要使用一次parse_line去分析，因此parse_headers中if_else的判断顺序不重要，需要多次进入parse_headers中去检验key-value
    */ 
    if(text[0] == '\0') // 首字符是'\0'只会出现在空行中
    {
        if(m_content_length!=0) //post请求
        {
            m_check_state = CHECK_STATE_CONTENT;//post请求需要检查还需要请求体
            return NO_REQUEST;
        }
        else
            return GET_REQUEST; //如果是get请求的话，此时完成分析任务了（此时还没有考虑带参的get请求）
    }
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text+=11;
        text+=strspn(text, " \t");
        if(strcasecmp(text,"keep-alive") == 0)
            m_linger = true;
    }
    else if(strncasecmp(text, "Content-length:", 15) == 0)
    {
        text+=15;
        text+=strspn(text, " \t");
        m_content_length = atoi(text);
    }
    else if(strncasecmp(text, "HOST:", 5) == 0)
    {
        text+=5;
        text+=strspn(text, " \t");
        m_host = text;  //服务器域名
    }
    // else
    // {
    //     LOG_INFO("oop!unknow header: %s", text);
    // }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;//NO_REQUEST表示请求未处理完
    char *text = 0;

    while((line_status = parse_line()) == LINE_OK)
    {
        text = getline();
        m_start_line = m_check_idx; //在parse_line中找到一行后，m_check_idx指向下一行的起始位置，因此要修改m_start_line
        LOG_INFO("%s", text);

        switch (m_check_state)  // m_check_state的初始值是CHECK_STATE_REQUESTLINE
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = analyze_request_line(text);
            if(ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
           ret = analyze_request_header(text);
        }    
        
        default:
            break;
        }
    }
}
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
}

bool http_conn::write()
{

}