/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __PROXY_H__
#define __PROXY_H__

/* General invocation interface. All fragments that make up one object
 * are accessed through a proxy which decides how to deal with the
 * invocation.
 */

#ifdef PURE_WRITE
#include "rts_types.h"

/*
 * An operation is a pure write when it satisfies two conditions:
 * 1. It only takes IN parameters    (OP_PURE_WRITE).
 * 2. It can never block on a guard  (OP_BLOCKING).
 */
int rts_is_pure_write(tp_dscr *obj_type, int opindex);

/*
 * rts_flush_pure_writes() blocks its caller when this caller wants to
 * perform an operation on a shared object while there are operations
 * pending on another shared object. If always != 0, then the caller
 * is always blocked, even if an operation on the same object is
 * attempted.
 */
void rts_flush_pure_writes(process_p self, fragment_p obj, int always);
#endif

extern void pr_start(void);

#endif
