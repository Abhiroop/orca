#ifndef __PAN_RPC_TICKET_H__
#define __PAN_RPC_TICKET_H__


#include "pan_sys_msg.h"



#ifndef STATIC_CI
#  define STATIC_CI	static
#endif



STATIC_CI void      pan_rpc_ticket_start(void);
STATIC_CI void      pan_rpc_ticket_end(void);

STATIC_CI short int pan_rpc_ticket_get(void);
STATIC_CI pan_msg_p pan_rpc_ticket_wait(short int ticket);
STATIC_CI void      pan_rpc_ticket_signal(short int ticket, pan_msg_p msg);


#endif
