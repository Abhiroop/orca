/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_global.h"

/* global variables */
pan_mutex_p pan_bg_lock;	/* global group lock */
pan_pset_p  pan_bg_all;		/* platform set containing all platforms */
seqno_t     pan_bg_rseqno;	/* last consecutively received message */
seqno_t     pan_bg_seqno;	/* sequencer sequence number */
seqno_t     pan_bg_ackno;	/* last ackno sent to sequencer */

