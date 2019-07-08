#ifndef __MCAST_FRAG_BUF_H__
#define __MCAST_FRAG_BUF_H__

#include "pan_sys.h"

/*
 * mcast: fragment & send, assemble & receive of multicast msgs.
 *
 * This module manages the fragment buffer: catch msgs are stored here until
 * the last fragment belonging to this msg has arrived.
 */



typedef struct frag_buf frag_buf_t, *frag_buf_p;



frag_buf_p frag_buf_create(int n_senders, short int n_tickets);

void       frag_buf_clear(frag_buf_p lst);

void       frag_buf_store(frag_buf_p lst, pan_msg_p msg, int sender,
			  short int ticket);

pan_msg_p  frag_buf_locate(frag_buf_p lst, int sender, short int ticket);


#endif
