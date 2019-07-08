#include <assert.h>

#include "pan_sys.h"

#include "mcast_ticket.h"


/*
 * mcast: fragment & send, assemble & receive of multicast msgs.
 *
 * This module implements a data structure to hand out/in tickets for
 * mcast msgs.
 */




typedef struct msg_list_item msg_link_t, *msg_link_p;

struct msg_list_item {
    msg_link_p     next;
    pan_msg_p      msg;
};

struct msg_list {
    msg_link_p     item;
    msg_link_p     free_list;
    pan_mutex_p    lock;
    pan_cond_p     available;
};


msg_list_p
msg_list_create(int size, pan_mutex_p lock)
{
    msg_list_p lst;
    int i;

    lst = pan_malloc(sizeof(msg_list_t));

    lst->item = pan_calloc(size, sizeof(msg_link_t));
    lst->free_list = &lst->item[0];
    for (i = 0; i < size - 1; i++) {
	lst->item[i].next = &lst->item[i+1];
    }
    lst->item[size - 1].next = NULL;
    lst->lock = lock;
    lst->available = pan_cond_create(lst->lock);

    return lst;
}

void
msg_list_clear(msg_list_p lst)
{
    pan_cond_clear(lst->available);
    pan_free(lst->item);
    pan_free(lst);
}

short int
msg_list_x_get(msg_list_p lst, pan_msg_p msg)
{
    short int ticket;

    pan_mutex_lock(lst->lock);
    while (lst->free_list == NULL) {
	pan_cond_wait(lst->available);
    }

    ticket = lst->free_list - &lst->item[0];
    assert(lst->free_list->msg == NULL);
    lst->free_list->msg = msg;

    lst->free_list = lst->free_list->next;

    if (lst->free_list != NULL) {
	pan_cond_signal(lst->available);
    }
    pan_mutex_unlock(lst->lock);

    return ticket;
}

void
msg_list_put(msg_list_p lst, short int ticket)
{
    msg_link_p ticket_slot;

    if (lst->free_list == NULL) {
	pan_cond_signal(lst->available);
    }
    ticket_slot = &lst->item[ticket];
    ticket_slot->next = lst->free_list;
    lst->free_list = ticket_slot;
#ifndef NDEBUG
    ticket_slot->msg = NULL;
#endif
}


pan_msg_p
msg_list_locate(msg_list_p lst, short int ticket)
{
    return lst->item[ticket].msg;
}
