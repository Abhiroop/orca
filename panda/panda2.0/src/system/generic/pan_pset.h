/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _SYS_GENERIC_PSET_
#define _SYS_GENERIC_PSET_

struct pan_pset{
    unsigned *mask;
};

extern void pan_sys_pset_start(void);
extern void pan_sys_pset_end(void);

#endif
