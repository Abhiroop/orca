/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __PAN_MSG_HDR_H__
#define __PAN_MSG_HDR_H__

#include <fm.h>

#include "pan_sys_msg.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


/* At the system layer, some headers are pushed onto a message:
 * 1. If the communication layer does piggy-backing of (some of) its state,
 *    it can push a header of size BCAST_HDR_SIZE or UCAST_HDR_SIZE,
 *    depending on the multicast flag of this nsap. These sizes are defined in
 *    the non-generic file pan_comm.h;
 * 2. The id of the nsap to which the message belongs. This id (of size
 *    NSAP_HDR_ID) is always at the end of a sent message.
 *
 * The following functions provide access.
 * Upon receipt of a message, always call pan_msg_restore()
 * immediately to obtain the nsap to which the msg corresponds.
 * This function has side-effects on the msg so be sure not to call it
 * twice!
 */

				/* Before sending out a msg, store:
				 * 1. communication layer hdr (if needs be)
				 * 2. nsap id. */
STATIC_CI int       pan_sys_msg_nsap_store(pan_msg_p msg, pan_nsap_p nsap);

				/* After network receive of a msg, restore
				 * the nsap id. The communication layer
				 * header can be looked. */
STATIC_CI pan_msg_p pan_msg_restore(struct FM_buffer *data, int len);


#endif
