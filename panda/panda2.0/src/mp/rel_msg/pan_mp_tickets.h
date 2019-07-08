#ifndef __PAN_MP_REL_MSG_TICKET_H__
#define __PAN_MP_REL_MSG_TICKET_H__


#include "pan_sys_msg.h"

#include "pan_mp_ports.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


typedef enum MP_FLAGS {
    MP_FREE		= 0,
    MP_ARRIVED		= 0x1 << 0
} mp_flags_t, *mp_flags_p;



struct MP_TICKET {
    mp_flags_t		flags;
    pan_msg_p		msg;
    pan_cond_p		arrived;
    mp_port_p		port;			/* Port for this ticket */
    mp_ticket_p		next;			/* Linked list support */
};




#ifndef STATIC_CI

void        mp_ticket_get(void);
void        mp_ticket_clear(mp_ticket_p p);

int         mp_ticket_id(mp_ticket_p ticket);
mp_ticket_p mp_ticket(int ticket_id);

void        mp_ticket_start(pan_mutex_p lock);
void        mp_ticket_end(void);

#endif


#endif
