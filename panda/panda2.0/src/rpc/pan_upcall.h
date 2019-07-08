/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_RPC_UPCALL__
#define __PAN_RPC_UPCALL__

#include "pan_rpc.h"

typedef struct pan_upcall{
    struct pan_upcall *next;
    int                dest;
    int                entry;
}pan_upcall_t;

void pan_rpc_upcall_start(void);
void pan_rpc_upcall_end(void);

pan_upcall_p pan_rpc_upcall_get(void);
void         pan_rpc_upcall_free(pan_upcall_p p);


#endif /* __PAN_RPC_UPCALL__ */
