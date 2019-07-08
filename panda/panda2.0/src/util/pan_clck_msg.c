/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*------ Clock message handling functions ------------------------------------*/

#include <stdio.h>

#include "pan_sys.h"

#include "pan_util.h"

#include "pan_clock_msg.h"






void
fill_clock_hdr(clock_hdr_p hdr, clock_msg_t type, int sender, int id,
	       int attempts)
{
    hdr->type = type;
    hdr->sender = sender;
    hdr->id = id;
    hdr->attempts = attempts;
}


clock_hdr_p
clock_hdr_pop(pan_msg_p msg)
{
    return pan_msg_pop(msg, sizeof(clock_hdr_t), alignof(clock_hdr_t));
}


clock_hdr_p
clock_hdr_look(pan_msg_p msg)
{
    return pan_msg_look(msg, sizeof(clock_hdr_t), alignof(clock_hdr_t));
}


clock_hdr_p
clock_hdr_push(pan_msg_p msg)
{
    return pan_msg_push(msg, sizeof(clock_hdr_t), alignof(clock_hdr_t));
}


void
print_clock_hdr(clock_msg_t tp)
{
    switch (tp) {
    case CLOCK_REQ:
	printf("CLOCK_REQ");
	break;
    case CLOCK_REPLY:
	printf("CLOCK_REPLY");
	break;
    }
}


void
print_clock_msg(pan_msg_p msg)
{
    clock_hdr_t h;
    clock_hdr_p hdr;
    pan_time_p  t = pan_time_create();
    pan_time_p  t0 = pan_time_create();

    h = *clock_hdr_pop(msg);
    print_clock_hdr(h.type);
    printf(" sender = %d id = %d attempt = %d ",
	   h.sender, h.id, h.attempts);
    tm_look_time(msg, t);
    printf("t0 = %f\n", pan_time_t2d(t));
    if (h.type == CLOCK_REPLY && pan_my_pid() > 0) {
	tm_pop_time(msg, t0);
	tm_look_time(msg, t);
	printf("t1 = %f\n", pan_time_t2d(t));
	tm_push_time(msg, t0);
    }
    hdr = clock_hdr_push(msg);
    *hdr = h;

    pan_time_clear(t0);
    pan_time_clear(t);
}
