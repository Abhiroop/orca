#include "pan_sys.h"		/* Provides a system interface */
#include "pan_global.h"
#include "pan_nsap.h"
#include "pan_system.h"
#include "pan_pset.h"
#include "pan_comm.h"
#include "pan_message.h"

#include <assert.h>

#ifndef MAX_SMALL_NSAPS
#define MAX_SMALL_NSAPS 16
#endif

#define MAX_NSAPS (2 * MAX_SMALL_NSAPS)

/*
 * The minidispatchers use pan_nsap_map to map an index in the range
 * [0 .. MAX_SMALL_NSAPS - 1] to a legal nsap.
 */
pan_nsap_p pan_nsap_map[MAX_SMALL_NSAPS];
int pan_small_nr;

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
    int ua = UNIVERSAL_ALIGNMENT - 1;

    assert(nsap->type == -1);

    nsap->mapid    = -1;
    nsap->type     = PAN_NSAP_FRAGMENT; 
    nsap->rec_frag = rec_frag;
    nsap->hdr_size = (header_size + ua) & ~ua;
    nsap->flags    = flags;
}

void 
pan_nsap_small(pan_nsap_p nsap, void rec_small(void *data), int len, int flags)
{
    assert(nsap->type == -1);
    assert(len <= MAX_SMALL_SIZE);
    assert(pan_small_nr < MAX_SMALL_NSAPS);

    nsap->mapid     = pan_small_nr++;
    nsap->type      = PAN_NSAP_SMALL; 
    nsap->rec_small = rec_small;
    nsap->data_len  = len;
    nsap->flags     = flags;

    /*
     * Register nsap in the map.
     */
    pan_nsap_map[nsap->mapid] = nsap;
}

pan_nsap_p 
pan_sys_nsap_id(int id)
{
    assert(id >= 0 && id < nr);

    return &nsaps[id];
}
