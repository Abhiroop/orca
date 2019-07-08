/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_MP_H__
#define __PAN_MP_H__

/*
  The message module provides reliable message passing. Its interface is
  especially targeted to the RPC implementation that can be put on top
  of it. However, it can also be used standalone. This module has two
  flavors:
  - for unreliable unicast. The MP layer implements a stop-and-wait
    protocol. 
  - for reliable unicast. The MP layer expects that unicast
    communication is FIFO. Note that this implementation has two
    versions:
    - one version uses a send daemon to send all fragments except the
      first one.
    - one version sends all remaining fragments from inside the upcall
      of the acknowledgement of the first fragment. This version is
      never tested, because on most systems, it can lead to deadlock.
 
  This module depends upon:
  \begin{itemize}
  \item the system module 
  \end{itemize}
 */


#include "pan_sys.h"


/* 
\section{Initialization and Termination}
*/
extern void pan_mp_init(void);
/*
	Initializes the message passing module. XXX: add pan\_mp\_start?
*/

extern void pan_mp_end(void);
/*
	Releases all resources held by the message passing module.
*/

/* 
\section{Maps}

Maps provide a means to address messages. Each higher-level module that
uses the mp module registers a map. The maps have to be registered in a
total order. A server can register a handler routine for a map it registered.
Messages for this map will than be forwarded to the corresponding handler
routine.
*/
#define DIRECT_MAP  0

extern int pan_mp_register_map(void);
extern void pan_mp_free_map(int map);
extern void pan_mp_clear_map(int map, void (*clear)(pan_msg_p message));



/* 
\section{Sending and Receiving}

XXX: naming conflict entry and map?
*/
#define MODE_SYNC  0
#define MODE_SYNC2 1
#define MODE_ASYNC 2

extern int pan_mp_send_message(int cpu, int entry, pan_msg_p message, 
			       int mode);
extern void pan_mp_finish_send(int entry);
extern int pan_mp_poll_send(int entry);
extern int pan_mp_receive_message(int map, pan_msg_p message, int mode);
extern void pan_mp_finish_receive(int entry);
extern int pan_mp_poll_receive(int entry);
extern void pan_mp_register_async_receive(int map, 
					  void (*handler)(int map, 
							  pan_msg_p msg));


#endif /* \_\_PAN\_MP\_H\_\_ */

