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
    users->init_mysql(m_connection_pool);
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

void Websever::timer(int connfd, struct sockaddr_in client_address)
{
    users[connfd].init(connfd,client_address, m_root, m_CONNTrigmode, m_close_log, m_user, m_password, m_databasename);
    
    //初始化对应的计时器
    util_timer * timer = new util_timer;
    timer->user_data = &user_timer[connfd];
    timer->callback_function = callback_function; //callback_function既可作为函数名，也可以作为函数指针
    time_t cur_time = time(NULL);
    timer->expire = cur_time + 3*TIMESLOT; //设置到期时间

    user_timer[connfd].address = client_address;
    user_timer[connfd].sockfd = connfd;
    user_timer[connfd].timer = timer;

    //将计时器加入到计时链表中
    utils.m_timer_lst.add_timer(timer);


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
        //调用timer方法，为这个用户连接创建计时器，并加入到容器中
        timer(connfd, client_address);
    }
    else
    //如果是边缘触发（有新的连接请求到来时才会报告读事件），那么需要一次性将缓冲区中的连接请求读完
    {
        while(1)
        {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &len);
            if(connfd < 0)
            {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                return false;            
            }
            if(http_conn::m_user_count >= MAX_FD)
            {
                utils.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                return false;
            }
            timer(connfd, client_address);
        }
    }
    return true;
}

void Websever::deal_timer(util_timer * timer, int sockfd)
{
    //调用回调函数，将用户连接的文件描述符删除
    timer->callback_function(&user_timer[sockfd]);
    if(timer)
    {
        //将与用户连接相关的定时器从定时器链表中删除
        utils.m_timer_lst.delete_timer(timer);
    }
    LOG_INFO("close fd %d", user_timer[sockfd].sockfd);
}
void Websever::adjust_timer(util_timer * timer)
{
    time_t now = time(NULL);
    timer->expire = now + 3*TIMESLOT;
    utils.m_timer_lst.adjust_timer(timer);
    LOG_INFO("%s", "adjust timer once");
}
bool Websever::deal_signal(bool & timeout, bool & stop_server)
{
    int ret = 0;
    char signal[1024];
    ret = recv(m_pipefd[0],signal, sizeof(signal), 0);
    if(ret == -1 || ret == 0)
        return false;
    for(int i=0; i<ret; i++)
    {
        switch (signal[i])
        {
        case SIGALRM:
            timeout = true;
            break;
        case SIGTERM:
            timeout = true;
            break;
        default:
            break;
        }
    }
    return true;
}
void Websever::deal_read(int sockfd)
{
    util_timer * timer = user_timer[sockfd].timer;

    //对于reactor，其读取请求数据是在任务线程中进行的
    if(m_actor_model == 1)
    {
        if(timer)
            adjust_timer(timer); //计时器加时
        m_threadpool->append(users + sockfd, 0); //添加到请求队列

        while(true)
        {
            if(users[sockfd].improv == 1) //表示数据读取完成
            {
                if(users[sockfd].timer_flag == 1) //timer_flag=1表示这个http连接出现问题，需要删除定时器
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }

        }
    }
    //对于proactor，对请求数据的读取是在主线程中进行的
    else
    {
        if (users[sockfd].read_once()) // 在主线程中使用read_once()函数读取接收缓冲区的请求数据
        {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            m_threadpool->append_p(users + sockfd); //添加到请求队列
            if (timer)
            {
                adjust_timer(timer);
            }
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }
}
void Websever::deal_write(int sockfd)
{
    util_timer * timer = user_timer[sockfd].timer;

    if(m_actor_model == 1)
    {
        if(timer)
            adjust_timer(timer);
        m_threadpool->append(users+sockfd, 1); //添加到请求队列

        while(true)
        {
            if(users[sockfd].improv == 1)
            {
                if(users[sockfd].timer_flag == 1)
                {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
            }
            users[sockfd].improv = 0;
            break;
        }
    }
    else
    {
        if(users[sockfd].write())
        {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            if(timer)
                adjust_timer(timer);
            
        }
        else
        {
            deal_timer(timer, sockfd);
        }
    }

}
void Websever::eventloop()
{
    bool timeout = false;   //
    bool stop_server = false;  //服务器状态：打开
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

        for(int i=0; i<number; i++)
        {
            int sockfd = events[i].data.fd;

            //用户连接请求
            if(sockfd == m_listenfd)
            {
                bool flag = deal_clentdata();
                continue;
            }
            //需要关闭已建立的连接
            else if(events[i].events & (EPOLLRDHUP || EPOLLHUP || EPOLLERR))
            {
                util_timer * timer = user_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            }
            //倒计时到时间后通过信号在进程间传递
            else if ((sockfd == m_pipefd[0]) && (events[i].events == EPOLLIN))
            {
                bool flag = deal_signal(timeout, stop_server);
                if (false == flag)
                    LOG_ERROR("%s", "dealclientdata failure");
            }
            // 不是上边的情况，那就说明时间发生在已经建立连接的客户连接上，处理客户数据就行
            //处理客户连接上接收到的数据
            else if(events[i].events & EPOLLIN)
            {
                deal_read(sockfd);
            }
            else if(events[i].events & EPOLLOUT)
            {
                deal_write(sockfd);
            }
        }
        if(timeout) //倒计时结束，处理定时器
        {
            utils.timer_handler();
            LOG_INFO("%s", "timer tick");
            timeout = false;
        }
    } 
    
}