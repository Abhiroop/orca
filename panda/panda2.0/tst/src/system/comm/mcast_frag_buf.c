#include <assert.h>

#include "pan_sys.h"

#include "mcast_frag_buf.h"

/*
 * mcast: fragment & send, assemble & receive of multicast msgs.
 *
 * This module manages the fragment buffer: catch msgs are stored here until
 * the last fragment belonging to this msg has arrived.
 */




struct frag_buf {
    pan_msg_p  *buf;
    int         n_x;
    int         n_y;
};

frag_buf_p
frag_buf_create(int n_senders, short int n_tickets)
{
    frag_buf_p f;

    f = pan_malloc(sizeof(frag_buf_t));
    f->n_x = n_senders;
    f->n_y = n_tickets;
    f->buf = pan_malloc(n_senders * n_tickets * sizeof(pan_msg_p));

    return f;
}

void
frag_buf_clear(frag_buf_p lst)
{
    pan_free(lst->buf);
    pan_free(lst);
}

void
frag_buf_store(frag_buf_p lst, pan_msg_p msg, int sender, short int ticket)
{
    assert(sender >= 0);
    assert(sender < lst->n_x);
    assert(ticket >= 0);
    assert(ticket < lst->n_y);
    lst->buf[sender * lst->n_y + ticket] = msg;
}

pan_msg_p
frag_buf_locate(frag_buf_p lst, int sender, short int ticket)
{
    assert(sender >= 0);
    assert(sender < lst->n_x);
    assert(ticket >= 0);
    assert(ticket < lst->n_y);
    return lst->buf[sender * lst->n_y + ticket];
}
