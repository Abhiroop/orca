#ifndef __MCAST_HDR_H__
#define __MCAST_HDR_H__


#include "pan_sys.h"

/*
 * mcast: fragment & send, assemble & receive of multicast msgs.
 *
 * This is the typedef of the mcast fragment header.
 */



typedef struct FRAG_HDR_T {
    int        sender;
    int        ticket;
    int        tag;
} frag_hdr_t, *frag_hdr_p;


#endif
