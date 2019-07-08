/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */


#ifndef __PAN_MCAST_RCVE_H__
#define __PAN_MCAST_RCVE_H__



#include "fm.h"				/* Fast Messages for Myrinet */

#include "pan_sys_msg.h"


#ifndef STATIC_CI
#  define STATIC_CI static
#endif



STATIC_CI void pan_mcast_data_upcall(pan_msg_p msg);
STATIC_CI void pan_mcast_data_handler(struct FM_buffer *data, int size);

#ifndef RELIABLE_NETWORK
STATIC_CI void pan_mcast_cntrl_upcall(pan_mcast_hdr_p hdr);
STATIC_CI void pan_mcast_cntrl_handler(struct FM_buffer *data, int size);
#endif

STATIC_CI void pan_mcast_rcve_init(void);
STATIC_CI void pan_mcast_rcve_end(void);


#endif
