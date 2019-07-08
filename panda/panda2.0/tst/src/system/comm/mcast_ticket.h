#ifndef __MCAST_TICKET_H__
#define __MCAST_TICKET_H__


#include "pan_sys.h"

/*
 * mcast: fragment & send, assemble & receive of multicast msgs.
 *
 * This module implements a data structure to hand out/in tickets for
 * mcast msgs.
 */



typedef struct msg_list msg_list_t, *msg_list_p;



msg_list_p msg_list_create(int size, pan_mutex_p lock);

void       msg_list_clear(msg_list_p lst);

short int  msg_list_x_get(msg_list_p lst, pan_msg_p msg);

void       msg_list_put(msg_list_p lst, short int ticket);

pan_msg_p  msg_list_locate(msg_list_p lst, short int ticket);

#endif
