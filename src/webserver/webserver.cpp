#include "webserver.h"




//默认构造
Websever::Websever()
{
    m_sqlUrl = "localhost";
    m_sqlPort = 3306;

    users = new http_conn[MAX_FD];

    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    user_timer = new client_data[MAX_FD];

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

void Websever::eventListen()
{
    //创建监听套接字，成功返回>=0
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);//使用assert函数检验文件描述符是否>=0，<0就打印错误并终止程序

    //设置连接关闭的方式
    if(0 == m_opt_linger)
    {
        struct linger tmp = {0, 1}; 
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if(1 == m_opt_linger)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    //创建ipv4监听地址
    struct sockaddr_in address; //需要<netinet/in.h>
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(m_port);

    int flag = 1;
    // 设置SO_REUSEADDR可以强制使用处于TIME_WAIT状态的连接占用的socket,在重启服务器时能马上重新绑上原来的端口
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

    //绑定IP地址和监听套接字
    int ret = bind(m_listenfd, (struct sockaddr *)&address, sizeof(address));
    assert(ret >= 0);

    //开始监听
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    //初始化定时器工具
    utils.init(TIMESLOT);


    //创建内核事件表
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    //将m_listened监听套接字向内核注册表中注册为读事件
    utils.addfd(m_epollfd, m_listenfd, false, m_LISTENTrigmode);
    http_conn::m_epollfd = m_epollfd;

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    utils.setnonblocking(m_pipefd[1]);
    utils.addfd(m_epollfd, m_pipefd[0], false, 0);

    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sig_handler, false);
    utils.addsig(SIGTERM, utils.sig_handler, false);

    //启动定时
    alarm(TIMESLOT);

    //Utils类的静态变量初始化
    Utils::u_pipefd = m_pipefd;
    Utils::u_epollfd = m_epollfd;
}


bool Websever::deal_clentdata()
{
    struct sockaddr_in client_address;
    socklen_t len = sizeof(client_address);

    /*如果监听描述符设置的是水平触发，则只要其缓冲区有东西就会触发，那么一次只需读一个连接就行*/
    if(0 == m_LISTENTrigmode)
    {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &len);
        if(connfd < 0)
        {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;            
        }
        //当连接数达到最大时，这个连接就不能被建立
        if(http_conn::m_user_count >= MAX_FD)
        {
            utils.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }

        timer()
    }
}

void Websever::eventloop()
{
    bool timout = false;
    bool stop_server = false;
    epoll_event events[MAX_EVENT_NUMBER];
    while (!stop_server)
    {
        /*在eventlisten函数中已经设置好了监听描述符，并将它添加到epoll中，注册的是读事件*/
        // -1表示无线等待，知道有事件发生
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);

        if(number < 0 && errno != EINTR)
        {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for(int i=0; i<number, i++)
        {
            int sockfd = events[i].data.fd;

            //用户连接
            if(sockfd == m_listenfd)
            {
                bool flag = deal_clentdata();
            }
        }
    } 
    
}