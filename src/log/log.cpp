#include "log.h"




Log::Log()
{
    m_count = 0;
    m_is_async = false;
}

Log::~Log()
{
    if(m_fp != NULL)
        fclose(m_fp);
}

bool Log::init(const char * file_name, int close_log, int log_buf_size, int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size，则为异步模式
    if(max_queue_size > 0)
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        // 创建异步写日志线程，执行函数
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }
    m_close_log = close_log;
    m_log_buf_size = log_buf_size;
    m_buf = new char[m_log_buf_size];
    memset(m_buf, '\0', m_log_buf_size);

    // 处理写入日志的文件名
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t); //获取本地时间，转换成tm类型
    struct tm my_tm = *sys_tm;

    const char *p = strchr(file_name, '/'); //获取最后一个'/'的位置
    char log_full_name[256] = {0};

    if(p==NULL) //说明参数file_name中没有'/',则file_name就是文件名
    {
        snprintf(log_full_name, 256, "%d_%02d_%02d_%s", my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, file_name);
    }
    else
    {
        strcpy(log_name, p+1); //复制文件名,从p的下一个位置开始复制
        strncpy(log_dirname, file_name, p-file_name+1);  //复制文件路径
        // 形如/home/../logname
        snprintf(log_full_name, 256, "%s_%d_%02d_%02d_%s", log_dirname, my_tm.tm_year+1900, my_tm.tm_mon+1, my_tm.tm_mday, log_name);
    }   

    m_today = my_tm.tm_mday;

    m_fp = fopen(log_full_name, "a"); //追加方式，如果文件不存在就新建一个文件
    if (m_fp == NULL)
    {
        return false;
    }
    return true;
}

void Log::write_log(int level, const char* format, ...)
{
    struct timeval now = {0,0}; 
    gettimeofday(&now, NULL); 
    time_t t = now.tv_sec; //自unix计时以来的秒数
    struct tm* sys_tm = localtime(&t); //将这个秒数转换成现在的时间
    struct tm my_tm = *sys_tm;
    // 日志类型
    char s[16] = {0};

    switch (level)
    {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[erro]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    // 对count和fp的值要修改，所以加锁
    m_mutex.lock();
    m_count++;
    // 当前的天和启动日志系统时不是一天，或者这一个日志文件的当前行数为m_split_lines，都需要再开一个日志文件
    if(m_today != my_tm.tm_mday || m_count % m_split_lines == 0)
    {
        char new_log[256] = {0}; // 新日志的文件名
        fflush(m_fp); //将缓冲区的字符全都flush进当前的日志文件，等会儿就要关闭了
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday);

        if(m_today != my_tm.tm_mday)
        {

            snprintf(new_log, 256, "%s%s%s", log_dirname, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        }
        else
        {
            snprintf(new_log, 256, "%s%s%s.%lld", log_dirname, tail, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    m_mutex.unlock();

    va_list valist;
    va_start(valist, format);

    string log_str;
    m_mutex.lock();

    // 写入的具体日志，将my_tm处理成字符串
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);

    int m = vsnprintf(m_buf+n, m_log_buf_size-n-1, format, valist);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    log_str = m_buf;
    m_mutex.unlock();
    //采用异步写
    /*
    如果采用异步写入，就必须写入阻塞队列，否则直接写入文件中的话，阻塞队列中还没写入的日志字符串会在之后写入
    ，但这会导致时间顺序上的错误
    */
    // if(m_is_async && m_log_queue->full())
    if(m_is_async)
    {
        m_log_queue->push(log_str);
    }
    else
    {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valist);
}

void Log::flush()
{
    m_mutex.lock();
    fflush(m_fp);
    m_mutex.unlock();
}