#ifndef __PAN_MP_REL_MSG_QUEUE_H__
#define __PAN_MP_REL_MSG_QUEUE_H__

#include "pan_sys_msg.h"

#include "pan_mp_ports.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifndef STATIC_CI

void        mp_q_enq(mp_ticket_p *q, mp_ticket_p p);
mp_ticket_p mp_q_deq(mp_ticket_p *q);

void        mp_q_init(mp_ticket_p *q);
void        mp_q_clear(mp_ticket_p *q, void (*clear)(pan_msg_p msg));

#endif


#endif
