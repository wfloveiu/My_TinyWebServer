#include "lst_timer.h"

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
        tmp->callback_function(); //!!
        
        // 此时这个tmp就是头节点，将它从链表中删除前，需要重置头节点
        head = tmp->next;
        // if(head)
        delete tmp;
        tmp = head;
    }
}