#ifndef __PAN_MCAST_NEXT_SEQNO_H__
#define __PAN_MCAST_NEXT_SEQNO_H__


/* Panda implementation of mcast_next_seqno.
 * As straightforward as possible:
 * - use a small nsap for this communication
 * - allow only 1 outstanding request at the time
 * - fixed server SEQNO_SERVER
 * - if client == server, get the sequence number directly
 */


#include "pan_global.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


#ifdef PANDA_NEXT_SEQNO

#ifndef STATIC_CI
void pan_mcast_begin_next_seqno(void);
int  pan_mcast_begin_next_seqno(void);
int  pan_mcast_next_seqno(void);
#endif


#define begin_next_seqno(tag,sz)	(pan_mcast_begin_next_seqno(), 0)
#define end_next_seqno(handle)		pan_mcast_end_next_seqno()
#define next_seqno(tag,sz)		pan_mcast_next_seqno()


#else		/* PANDA_NEXT_SEQNO */


#include <fm.h>


#define begin_next_seqno(tag,sz)	FM_begin_next_seqno_and_credits(tag,sz)
#define end_next_seqno(handle)		(FM_end_next_seqno(handle) - 1)
#define next_seqno(tag,sz)		(FM_next_seqno_and_credits(tag,sz) - 1)


#endif		/* PANDA_NEXT_SEQNO */


#ifndef STATIC_CI
void pan_mcast_next_seqno_start(void);
void pan_mcast_next_seqno_end(void);
#endif


#endif
