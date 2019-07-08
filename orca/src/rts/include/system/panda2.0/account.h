/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include "orca_types.h"

#ifdef ACCOUNTING

extern void ac_start();
extern void ac_end();

extern void ac_init( fragment_p f);
extern void ac_clear( fragment_p f);

extern void ac_tick( fragment_p f, access_t kind, source_t source);

#else

#define ac_start()
#define ac_end()

#define ac_init( f)
#define ac_clear( f)
#define ac_tick( f, kind, source)	\
	do { \
		if ( kind == AC_READ && f_get_status(f) == f_replicated) { \
			f->fr_info.delta_reads++; \
		} \
		man_tick( f, source == AC_LOCAL_BC ||\
			     source == AC_REMOTE_BC || kind == AC_WRITE); \
	} while (0)

#endif		/* ACCOUNTING */

#endif
