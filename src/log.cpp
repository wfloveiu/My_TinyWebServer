#include "log.h"

Log::Log()
{
    m_present_count = 0;
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
        pthread_create(&tid, NULL, xxxxx, NULL);
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