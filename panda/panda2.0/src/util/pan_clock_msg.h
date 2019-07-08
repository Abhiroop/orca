/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*------ Clock message handling functions ------------------------------------*/


#ifndef __UTIL_PAN_CLOCK_MSG_H__
#define __UTIL_PAN_CLOCK_MSG_H__

#include "pan_sys.h"




/*------ Types: clock msg type type, clock hdr type --------------------------*/



typedef enum CLOCK_MSG_T {
    CLOCK_REQ,
    CLOCK_REPLY
} clock_msg_t, *clock_msg_p;


typedef struct CLOCK_HDR_T clock_hdr_t, *clock_hdr_p;

struct CLOCK_HDR_T {
    clock_msg_t type;
    int         sender;
    int         id;			/* Identify sync round */
    int         attempts;		/* Attempt within sync round */
};


/*------ Functions ----------------------------------------------------------*/


void        fill_clock_hdr(clock_hdr_p hdr, clock_msg_t type, int sender,
			   int id, int attempts);

clock_hdr_p clock_hdr_pop(pan_msg_p msg);

clock_hdr_p clock_hdr_look(pan_msg_p msg);

clock_hdr_p clock_hdr_push(pan_msg_p msg);

void        print_clock_hdr(clock_msg_t tp);

void        print_clock_msg(pan_msg_p msg);

#endif
