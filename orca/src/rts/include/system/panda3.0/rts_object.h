/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __RTS_OBJECT_H__
#define __RTS_OBJECT_H__

#include "orca_types.h"
#include "rts_types.h"

#define RO_NO_OWNER   -1

extern void ro_init(fragment_p obj, char *name);

extern void ro_clear(fragment_p obj);

#endif
