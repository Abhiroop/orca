#ifndef __PAN_MP_REL_MSG_PORT_H__
#define __PAN_MP_REL_MSG_PORT_H__

#include <limits.h>

#include "pan_sys_msg.h"



#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#define MP_MAX_PORT_VALUE	((SHRT_MAX / 2) + 1)


typedef struct MP_TICKET mp_ticket_t, *mp_ticket_p;


typedef struct MP_PORT mp_port_t, *mp_port_p;

struct MP_PORT {
    void	      (*handler)(int port_id, pan_msg_p msg);
    pan_nsap_p		port_nsap;
    mp_ticket_p		server_q;		/* Register server threads */
    mp_ticket_p		arrived_q;		/* Register arrived messages */
};



#ifndef STATIC_CI

mp_port_p mp_get_port(void);
int       mp_port_id(mp_port_p p);
mp_port_p mp_port(int port_id);
void      mp_port_start(pan_mutex_p lock);
void      mp_port_end(void);

#endif


#endif
