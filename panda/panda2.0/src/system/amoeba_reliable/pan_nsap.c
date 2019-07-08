#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_nsap.h"
#include "pan_system.h"
#include "pan_pset.h"
#include "pan_comm.h"
#include "pan_message.h"

#include <assert.h>

#define MAX_NSAPS 32

static struct pan_nsap nsaps[MAX_NSAPS];
static int             nr;

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

    assert(nr < MAX_NSAPS);
    assert(!pan_sys_started);

    nsap = &nsaps[nr];

    nsap->type     = -1;
    nsap->nsap_id  = nr;
    nsap->rec_frag = NULL;
    nsap->rec_small  = NULL;
    nsap->hdr_size = -1;
    nsap->data_len = -1;

    nr++;

    return nsap;
}

void 
pan_nsap_clear(pan_nsap_p nsap)
{
}

void 
pan_nsap_fragment(pan_nsap_p nsap, void rec_frag(pan_fragment_p frag),
		  int header_size, int flags)
{
    assert(nsap->type == -1);

    nsap->type     = PAN_NSAP_FRAGMENT; 
    nsap->rec_frag = rec_frag;
    nsap->hdr_size = header_size;
    nsap->flags    = flags;
    if (flags & PAN_NSAP_MULTICAST) {
	nsap->comm_hdr_size = BCAST_COMM_HDR_SIZE;
    } else {
	nsap->comm_hdr_size = UCAST_COMM_HDR_SIZE;
    }
}

void 
pan_nsap_small(pan_nsap_p nsap, void rec_small(void *data), int len, int flags)
{
    assert(nsap->type == -1);
    assert(len <= MAX_SMALL_SIZE);

    nsap->type      = PAN_NSAP_SMALL; 
    nsap->rec_small = rec_small;
    nsap->data_len  = len;
    nsap->flags     = flags;
    if (flags & PAN_NSAP_MULTICAST) {
	nsap->comm_hdr_size = BCAST_COMM_HDR_SIZE;
    } else {
	nsap->comm_hdr_size = UCAST_COMM_HDR_SIZE;
    }
}

pan_nsap_p 
pan_sys_nsap_id(int id)
{
    assert(id >= 0 && id < nr);

    return &nsaps[id];
}
 

void pan_sys_nsap_comm_hdr(pan_nsap_p nsap, int comm_hdr_size)
{
    nsap->comm_hdr_size = comm_hdr_size;
}
