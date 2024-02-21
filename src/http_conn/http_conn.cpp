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

//将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void http_conn::init_mysql(connection_pool * connPoll)
{
    MYSQL * mysql  = NULL;
    connectionRAII mysqlcon(&mysql, connPoll);

    //检索user表中的username和password
    if(mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }
    MYSQL_RES * result = mysql_store_result(mysql);
    while(MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}
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

//分析完请求报文后，对不同的请求做出对应的处理
http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int root_len = strlen(doc_root);
    //将浏览器中的网址抽象为ip:port/xxx,m_url就是/xxx
    const char * p = strrchr(m_url, '/');
    
    /*
        项目中解析后的m_url有8种情况
        1. / 
            GET请求，跳转到judge.html，即欢迎访问页面
        2. /0
            POST请求，跳转到register.html，即注册页面
        3. /1
            POST请求，跳转到log.html，即登录页面
        4. /2CGISQL.cgi
            POST请求，进行登录校验,验证成功跳转到welcome.html，即资源请求成功页面,验证失败跳转到logError.html，即登录失败页面
        5. /3CGISQL.cgi
            POST请求，进行注册校验,注册成功跳转到log.html，即登录页面,注册失败跳转到registerError.html，即注册失败页面
        6. /5
            POST请求，跳转到picture.html，即图片请求页面
        7. /6
            POST请求，跳转到video.html，即视频请求页面
        8. /7
            POST请求，跳转到fans.html，即关注页面
    */
    if(cgi == 1 && ((*(p+1) == '2') || (*(p+1) == '3')))
    {
        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        for(i=5;m_string[i]!='&';i++)
            name[i-5] = m_string[i];
        name[i-5] = '\0';
        for(int j = i+8; m_string[j]!='\0';j++)
            password[j-i-8] = m_string[j];
        
        if(*(p+1) == '3')
        {
            char * sql_insert = (char *)malloc(sizeof(char) * 200);
            sprintf(sql_insert, "INSERT INTO user(username, passwd) VALUES('%s', '%s')", name, password);
            if(users.find(name) == users.end())
            {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string,string>(name, password));
                m_lock.unlock();

                if(!res)
                    strcpy(m_url, "/log.html");
                else   
                    strcpy(m_url, "/registerError.html");
            }
            else   
                strcpy(m_url, "/registerError.html");
        }
        else if(*(p+1) == '2')
        {
            if(users.find(name)!=users.end() && users[name]==password)
                strcpy(m_url, "/welcome.html");
            else    
                strcpy(m_url, "/logError.html");
        }
        strncpy(m_real_file+root_len, m_url, strlen(m_url));
    }

    if(*(p+1) == '0')
    {
        char * temp = (char *)malloc(sizeof(char) * 200);
        strcpy(temp, "/register.html");
        strncpy(m_real_file+root_len,temp, strlen(temp));
        free(temp);
    }
    else if(*(p+1) == '1')
    {
        char * temp = (char *)malloc(sizeof(char) * 200);
        strcpy(temp, "/log.html");
        strncpy(m_real_file+root_len,temp, strlen(temp));
        free(temp);
    }
    else if(*(p+1) == '5')
    {
        char * temp = (char *)malloc(sizeof(char) * 200);
        strcpy(temp, "/picture.html");
        strncpy(m_real_file+root_len,temp, strlen(temp));
        free(temp);
    }
    else if(*(p+1) == '6')
    {
        char * temp = (char *)malloc(sizeof(char) * 200);
        strcpy(temp, "/video.html");
        strncpy(m_real_file+root_len,temp, strlen(temp));
        free(temp);
    }
    else if(*(p+1) == '7')
    {
        char * temp = (char *)malloc(sizeof(char) * 200);
        strcpy(temp, "/fans.html");
        strncpy(m_real_file+root_len,temp, strlen(temp));
        free(temp);
    }
    else
        strncpy(m_real_file+root_len, m_url, strlen(m_url)); //m_url为"/judge.html"时

    if(stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE; //文件不存在
    if(!(m_file_stat.st_mode & S_IROTH)) //S_IROTH表示其它用户具有访问权限
        return FORBIDDEN_REQUEST;
    if(S_ISDIR(m_file_stat.st_mode)) //检查是否是一个目录
        return BAD_REQUEST;

    int fd = open(m_real_file, O_RDONLY); //只读打开文件
    m_file_address = (char *)mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

http_conn::HTTP_CODE http_conn::analyze_content(char *text)
{
    if(m_read_idx >= m_content_length + m_check_idx) //判断请求体是否完整
    {
        text[m_content_length] = '\0';
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}
http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;//NO_REQUEST表示请求未处理完
    char *text = 0;

    while( (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK)
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
            if(ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if(ret == GET_REQUEST)
                return  do_request();
            break;
        }    
        case CHECK_STATE_CONTENT:
        {
            ret = analyze_content(text);
            if(ret == GET_REQUEST)
                return do_request(); 
            line_status = LINE_OPEN;
            break;

        }
        default:
            return INTERNAL_ERROR;
        }
    }
}

bool http_conn::add_response(const char * format, ...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf+m_write_idx, WRITE_BUFFER_SIZE-1-m_write_idx, format, arg_list); 
    if(len > WRITE_BUFFER_SIZE-1-m_write_idx)
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx+=len;
    va_end(arg_list);
    LOG_INFO("request:%s", m_write_buf);
    return true;
}

bool http_conn::add_status_line(int status, const char * title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title); //如HTTP/1.1 200 OK
}
bool http_conn::add_header(int conten_len)
{
    /*在响应头中生成：内容长度、链接状态，并生成空行*/
    return add_content_length(conten_len) && add_linger() && add_blank_line();
}
bool http_conn::add_content(const char * content)
{
    return add_response("%s\r\n",content);
}
bool http_conn::add_content_length(int conten_len)
{
    return add_response("Content-Length:%d\r\n", conten_len);
}
bool http_conn::add_linger()
{
    return add_response("Connectino:%s\r\n", (m_linger == true)?"keep-alive":"close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

bool http_conn::process_write(HTTP_CODE read_ret)
{
    switch (read_ret)
    {
    case INTERNAL_ERROR:
    {
        if  (add_status_line(500,error_500_title) &&  
            add_header(strlen(error_500_form)) &&
            add_content(error_500_form))
            break;
        else
            return false;
    }    
    case BAD_REQUEST:
    {
        if  (add_status_line(404,error_404_title) &&
            add_header(strlen(error_404_title)) &&
            add_content(error_404_form))
            break;
        else
            return false;
    }
    case FORBIDDEN_REQUEST: //服务器禁止访问
    {
        if  (add_status_line(403,error_404_title) &&
            add_header(strlen(error_403_title)) &&
            add_content(error_403_form))
            break;
        else
            return false;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if(!m_file_stat.st_size)
        {
            add_header(m_file_stat.st_size);
            //第一个iovec指针指向响应报文缓冲区
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;

            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;

            m_iv_count = 2;
            byte_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char * ok_string = "<html><body></body></html>";
            if(add_header(strlen(ok_string)) && add_content(ok_string))
                break;
            else
                return false;
        }
    }
    default:
        return false;
    }
    /*除了200之外，其它状态不需要输出html文档，只用一个m_iv输出buffer内容即可*/
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    byte_to_send = m_write_idx;
    return true;
}


void http_conn::close_conn(bool real_close)
{
    if(real_close && m_sockfd!=-1)
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void http_conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
bool http_conn::write()
{
    int temp = 0;
    
    while(1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);
        if(temp < 0)
        {
            // 判断缓冲区是否满了，如果eagain，则是缓冲区满了，注册写事件，等待下一次触发，因此在此期间无法立即接收统同一用户的下一个请求
            if(errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, 0);
                return true;
            }
            else
            {
                unmap();
                return false;
            }
        }
        byte_have_send += temp;
        byte_to_send -= temp;

        // 每次调用writev成功后，都需要根据成功写入的字节数，调整iovec的io_base和io_len
        if(byte_have_send < m_iv[0].iov_len)
        {
            m_iv[0].iov_base += temp;
            m_iv[0].iov_len -= temp;
        }
        else
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base += byte_have_send-m_write_idx;
            m_iv[1].iov_len -= byte_to_send;
        }
        //数据完成发送
        if(byte_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);//重置为监听读事件
            if(m_linger)
            {
                init(); //重新初始化所有参数，用于下次客户端访问
                return true;
            }
            return false; //对于不是保持长连接，直接返回false后删除与改socket相关的文件描述符
        }
    }
}
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    //NO_REQUEST，表示请求不完整，需要继续接收请求数据
    if(read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    bool write_ret = process_write(read_ret);
    if(!write_ret)
        close_conn();
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);   
    
}

