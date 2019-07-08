/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_FRAGMENT_HDR_H__
#define __PAN_FRAGMENT_HDR_H__

#include <fm.h>

#include "pan_sys.h"		/* for types */


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


/* At the system layer, some headers are pushed onto a fragment:
 * 1. If the communication layer does piggy-backing of (some of) its state,
 *    it can push a header of size BCAST_HDR_SIZE or UCAST_HDR_SIZE,
 *    depending on the multicast flag of this nsap. These sizes are defined in
 *    the non-generic file pan_comm.h;
 * 2. The id of the nsap to which the fragment belongs. This id (of size
 *    NSAP_HDR_ID) is always at the end of a sent fragment.
 *
 * The following functions provide access.
 * Upon receipt of a fragment, always call pan_fragment_restore()
 * immediately to obtain the nsap to which the fragment corresponds.
 * This function has side-effects on the fragment so be sure not to call it
 * twice!
 */

#ifndef STATIC_CI

				/* After network receive of a fragment, restore:
				 * 1. communication layer hdr (if needs be)
				 * 2. nsap id. */
extern void pan_fragment_restore(pan_fragment_p frag, struct FM_buffer *data, int len);

				/* Before sending out a fragment, store:
				 * 1. nsap id
				 * 2. communication layer hdr (if needs be) */
extern int  pan_sys_fragment_nsap_store(pan_fragment_p frag);


#endif


#endif
