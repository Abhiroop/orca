/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_INTERNALS_H__
#define __RTS_INTERNALS_H__

/* $Id: rts_internals.h,v 1.1 1998/06/11 12:00:52 ceriel Exp $ */

#include "orca_types.h"

void rts_score(void *data, tp_dscr *d, double score, double naccess,
                      double uncertainty, int lock);
void rts_erocs(void *data, tp_dscr *d, double score, double naccess,
                      double uncertainty, int lock);

extern int rts_prepare_object_marshall(fragment_p obj);
int o_shared_nbytes(void *arg, tp_dscr *descr, int cpu);
pan_iovec_p o_shared_marshall(pan_iovec_p p, void *arg, tp_dscr *descr, int cpu);
void o_shared_unmarshall(pan_msg_p p, void **arg, tp_dscr *descr);
int rtspart_nbytes(t_object *arg, tp_dscr *descr, int flags);
char *rtspart_marshall(char *p, t_object *arg, tp_dscr *descr, int flags);
void rtspart_unmarshall(pan_msg_p p, t_object *arg, tp_dscr *descr, int flags);

#endif
