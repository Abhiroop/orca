/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>

#include "pan_sys_msg.h"		/* Provides a system interface */

#include "pan_global.h"
#include "pan_nsap.h"
#include "pan_system.h"
#include "pan_pset.h"
#include "pan_comm.h"
#include "pan_message.h"


struct pan_nsap pan_sys_nsaps[MAX_NSAPS];
int             pan_sys_nsap_nr;

void
pan_sys_nsap_start(void)
{
}

void
pan_sys_nsap_end(void)
{
}


pan_nsap_p 
pan_nsap_create(void)
{
    pan_nsap_p nsap;

    assert(!pan_sys_started);
    if (pan_sys_nsap_nr >= MAX_NSAPS) {
	pan_panic( "max number of nsaps = %d", MAX_NSAPS);
    }

    nsap = &pan_sys_nsaps[pan_sys_nsap_nr];

    nsap->type      = 0;
    nsap->nsap_id   = pan_sys_nsap_nr;
    nsap->rec_msg   = NULL;
    nsap->rec_small = NULL;
    nsap->data_len  = -1;

    pan_sys_nsap_nr++;

    return nsap;
}

void 
pan_nsap_clear(pan_nsap_p nsap)
{
}

void 
pan_nsap_msg(pan_nsap_p nsap, void rec_msg(pan_msg_p msg), int flags)
{
    assert(nsap->type == 0);

    nsap->type     |= PAN_NSAP_FRAGMENT; 
    nsap->rec_msg  = rec_msg;
    nsap->flags    = flags;

    if (flags & PAN_NSAP_MULTICAST) {
	nsap->type |= PAN_NSAP_MULTICAST; 
	nsap->comm_hdr_size = BCAST_HDR_SIZE;
    } else {
	nsap->type |= PAN_NSAP_UNICAST; 
	nsap->comm_hdr_size = UCAST_HDR_SIZE;
    }
}

void 
pan_nsap_small(pan_nsap_p nsap, void rec_small(void *data), int len, int flags)
{
    assert(nsap->type == 0);
    assert(len <= MAX_SMALL_SIZE);

    nsap->type      |= PAN_NSAP_SMALL; 
    nsap->rec_small = rec_small;
    nsap->data_len  = len;
    nsap->flags     = flags;

    if (flags & PAN_NSAP_MULTICAST) {
	nsap->type |= PAN_NSAP_MULTICAST; 
	nsap->comm_hdr_size = BCAST_HDR_SIZE;
    } else {
	nsap->type |= PAN_NSAP_UNICAST; 
	nsap->comm_hdr_size = UCAST_HDR_SIZE;
    }
}
