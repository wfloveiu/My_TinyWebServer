#include "lst_timer.h"
#include "../http_conn/http_conn.h"


sort_timer_lst::sort_timer_lst():head(NULL), tail(NULL)
{

}

void sort_timer_lst::add_timer(util_timer * timer)
{
    if(!timer)
        return;
    if(!head)
    {
        head = tail = timer;
        return;
    }
    //不存在头节点，因此对插入位置要分类讨论
    if(timer->expire <= head->expire) //插在头指针之后
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    else   
        add_timer(timer, head);
}

//不将这个函数写在上边函数内部，而是分出来
void sort_timer_lst::add_timer(util_timer * timer, util_timer * lst_header)
{
    util_timer * prev = lst_header; //前一个节点
    util_timer * tmp = lst_header->next;//当前判断节点

    while(tmp)
    {
        if(tmp->expire > timer->expire)
        {
            timer->next = tmp;
            tmp->prev = timer;
            prev->next = timer;
            timer->prev = prev;
            break;
        }
        tmp = tmp->next;
    }

    // 比这个链表中的最后一个的定时时间都晚，则放在最后作为尾节点
    if(!tmp)
    {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
    
}
/*当某个定时任务发生变化时，调整对应的定时器在链表中的位置。这个函数只考虑被
调整的定时器的超时时间延长的情况，即该定时器需要往链表的尾部移动*/
void sort_timer_lst::adjust_timer(util_timer * timer)
{
    if(!timer)
        return;
    util_timer * tmp = timer->next;
    //如果改时间后，超时值仍然比下一个定时器的超时值小，则不需要变位置
    if(timer->expire < tmp->expire)
        return;
    //是链表头，则取出来从头重新插入
    if(timer == head)
    {
        head = head->next;
        head->prev  = NULL;
        timer->next = NULL;
        add_timer(timer);
    }
    //如果是尾节点，也不需要变位置
    else if(timer->next == NULL)
        return;
    //如果是中间的某个节点，则取出来，调用add_timer往后边插
    else
    {
        timer->prev->next = tmp;
        tmp->prev = timer->prev;
        timer->prev = NULL;
        timer->next = NULL;
        add_timer(timer, tmp);
    }
}

void sort_timer_lst::delete_timer(util_timer * timer)
{
    if(!timer)
        return;
    if((timer == head) && (tail == timer))
    {
        delete timer;
        head = tail = NULL;
        return;
    }
    if(timer == head)
    {
        head = timer->next;
        timer->next->prev = NULL;
        delete timer;
        return;
    }
    if(timer == tail)
    {
        tail = timer->prev;
        timer->prev->next = NULL;
        delete timer;
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void sort_timer_lst::tick()
{
    if(!head)
        return;
    time_t cur = time(NULL); //获取当前系统绝对时间
    util_timer * tmp = head;
    while(tmp)
    {
        if(cur < tmp->expire)
            break;
        tmp->callback_function(tmp->user_data); //!!
        
        // 此时这个tmp就是头节点，将它从链表中删除前，需要重置头节点
        head = tmp->next;
        // if(head)
        delete tmp;
        tmp = head;
    }
}
void Utils::init(int timeslot)
{
    m_TIMESLOT = timeslot;
}

int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return new_option;
}

void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMod)
{
    epoll_event event;
    event.data.fd = fd;

    if(1 == TRIGMod) //1表示ET边沿触发
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP; //读事件，边沿触发
    else    
        event.events = EPOLLIN | EPOLLRDHUP;
    
    //同一时刻只能有一个进程处理这个文件，对于监听描述符，不能设置这个
    if(one_shot)
        event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    //ET还是LT，这里都是把它设置成非阻塞
    setnonblocking(fd);
}


void Utils::timer_handler()
{
    m_timer_lst.tick();
    alarm(m_TIMESLOT);
}

void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    /*
    sa_mask指定在信号处理的过程中哪些信号被阻塞
    这里使用sigfillset将所有的信号加入sa_mask，从而在信号处理时阻塞所有信号
    */ 
   sigfillset(&sa.sa_mask);
   // sigaction函数用于注册信号
   assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::sig_handler(int sig)
{   
    int save_errno = errno;
    int msg = sig;
    //向管道接收端发送信号
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void Utils::show_error(int connfd, const char * info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}
//静态变量初始化
int * Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

//实现callback函数
class Utils;
void callback_function(client_data *user_data)
{
    //删除掉epoll中这个用户的文件描述符
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    //关闭连接
    close(user_data->sockfd);

    http_conn::m_user_count--;
}