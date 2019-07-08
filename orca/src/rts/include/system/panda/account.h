#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include "orca_types.h"

#ifdef ACCOUNTING

PUBLIC void ac_start();
PUBLIC void ac_end();

PUBLIC void ac_init( fragment_p f);
PUBLIC void ac_clear( fragment_p f);

PUBLIC void ac_tick( fragment_p f, access_t kind, source_t source);

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
		man_tick( f, kind == AC_WRITE); \
	} while (0)

#endif ACCOUNTING

#endif
