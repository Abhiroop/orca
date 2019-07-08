/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PANDA_RPC_H__
#define __PANDA_RPC_H__

/*
   The RPC module provides reliable RPC with at-most-once semantics.

   This module depends upon:
   \begin{itemize}
   \item the system module
   \item the message passing module
   \end{itemize}
*/

#include "pan_sys.h"
#include "pan_mp.h"

typedef struct pan_upcall *pan_upcall_p;

typedef void (*pan_rpc_handler_f)(pan_upcall_p upcall, pan_msg_p request);


extern void pan_rpc_init(void);
extern void pan_rpc_end(void);
extern void pan_rpc_register(pan_rpc_handler_f handler);
extern void pan_rpc_trans(int dest, pan_msg_p request, pan_msg_p *reply);
extern void pan_rpc_reply(pan_upcall_p upcall, pan_msg_p reply);

#endif /* \_\_PANDA\_RPC\_H\_\_ */
