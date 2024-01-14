/*定时器类，游双Linux高性能服务器编程第11章*/
#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <time.h>

class util_timer;//前向声明，因为在client_data中使用了util_timer类

struct client_data
{

};

/*定时器类包括：超时时间、连接资源、定时事件
超时时间 = 浏览器和服务器连接时刻 + 固定时间，是绝对时间
连接资源
定时事件是回调函数，即删除非活动socket上的注册事件
*/
class util_timer
{
public:
    util_timer():prev(NULL), next(NULL){}
public:
    time_t expire;
    //回调函数
    void(*callback_function)(client_data *);
    client_data * user_data;
    util_timer * prev;
    util_timer * next;
};

/*定时器容器，用双向链表将定时器组织起来*/
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();
    void add_timer(util_timer * timer);
    void adjust_timer(util_timer * timer);
    void delete_timer(util_timer * timer);
    /*SIGALRM每被触发一次，就调用一次定时任务处理函数，处理链表中到期的定时器*/
    void tick();
private:
    void add_timer(util_timer * timer, util_timer * list_head);
    util_timer * head;
    util_timer * tail;
};

class Utils
{
public:
    Utils();
    ~Utils();
public:
    static int *
}
#endif