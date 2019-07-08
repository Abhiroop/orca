/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_INTERNALS_H__
#define __RTS_INTERNALS_H__

/* $Id: rts_internals.h,v 1.8 1996/07/15 11:45:47 ceriel Exp $ */

#include "orca_types.h"

extern int rts_prepare_object_marshall(fragment_p obj);
int o_shared_nbytes(void *arg, tp_dscr *descr, int cpu);
char *o_shared_marshall(char *p, void *arg, tp_dscr *descr, int cpu);
char *o_shared_unmarshall(char *p, void **arg, tp_dscr *descr);
int rtspart_nbytes(t_object *arg, tp_dscr *descr, int flags);
char *rtspart_marshall(char *p, t_object *arg, tp_dscr *descr, int flags);
char *rtspart_unmarshall(char *p, t_object *arg, tp_dscr *descr, int flags);
#endif
