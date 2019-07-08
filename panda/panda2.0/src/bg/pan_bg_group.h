/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __BIGGRP_GROUP_H__
#define __BIGGRP_GROUP_H__

#define SEQNO_BB       0
#define SEQNO_PB       1
#define SEQNO_START    2

typedef unsigned seqno_t;

typedef struct{
    seqno_t seqno;  /* the sequence number of this fragment */
    
    int  pid;       /* identification of the sender of the fragment */
    
    seqno_t index;  /* the sender's sequence number of this fragment */

    int dest;       /* the destination entry of this fragment */

    int src;	    /* the source entry of the fragment */

    seqno_t ackno;  /* acknowledges the highest sequence number received */
}pan_bg_hdr_t, *pan_bg_hdr_p;

#define DEST_ANONYMOUS -1

extern int pan_bg_local(pan_fragment_p fragment, pan_bg_hdr_p header, 
			int in_hist);

#endif
