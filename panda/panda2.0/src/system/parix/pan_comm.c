#include <sys/sem.h>
#include <stdlib.h>
#include <string.h>
#include <sys/root.h>
#include <sys/link.h>
#include <sys/time.h>
#include <sys/memory.h>
#include <assert.h>

#include "pan_sys.h"

#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"
#include "pan_nsap.h"

#include "pan_comm.h"
#include "pan_fragment.h"
#include "pan_comm_inf.h"
#include "pan_bcst_seq.h"
#include "pan_bcst_fwd.h"
#include "pan_bcst_snd.h"
#include "pan_ucast.h"
#include "pan_msg_cntr.h"
#include "pan_free_lnk.h"


trc_event_t  pan_trc_start_upcall;
trc_event_t  pan_trc_end_upcall;


int         pan_sys_x;
int         pan_sys_y;			/* Node coordinates of this processor */

LinkCB_t   *pan_bcast_req_link;
LinkCB_t   *pan_bcast_data_link;

Semaphore_t pan_comm_req_lock;		/* request seq numbers one by one */
Semaphore_t pan_comm_upcall_lock;	/* make upcalls one by one */




pan_fragment_p
pan_comm_small2frag(void *data, pan_nsap_p nsap)
{
    pan_fragment_p frag;

    frag = pan_fragment_create();
    if (nsap->data_len > 0) {
	memcpy(frag->data, data, nsap->data_len);
    }
    frag->size  = nsap->data_len;
    frag->nsap  = nsap;

    return frag;
}



static void
link_to_seq(void)
{
    int mem_before;
    int mem_after;
    int error;

    if (pan_sys_Parix_id != pan_sys_sequencer && pan_sys_link_connected) {
	ChangePriority(HIGH_PRIORITY);

	mem_before = mallinfo().freearena;
#if (defined PARIX_T800) && (! defined NO_EXHAUST_VLINKS)
	exhaust_vlinks();	/* fool PARIX! */
#endif
	mem_after = mallinfo().freearena;
	pan_comm_info_set(MEM_SLOT, (pan_sys_mem_startup + mem_after -
					mem_before) / 1024);

	pan_bcast_req_link = MakeLink(pan_sys_sequencer, STAR_TOP_1, &error);
	if (pan_bcast_req_link == NULL) {
	    pan_panic("MakeLink failed");
	}
	pan_bcast_data_link = MakeLink(pan_sys_sequencer, STAR_TOP_2, &error);
	if (pan_bcast_data_link == NULL) {
	    pan_panic("MakeLink failed");
	}

	ChangePriority(LOW_PRIORITY);
    } else {
	pan_comm_info_set(MEM_SLOT, pan_sys_mem_startup / 1024);
    }
}



void
pan_sys_comm_start(int n_servers)
{
    assert(aligned(sizeof(bcast_hdr_t), UNIVERSAL_ALIGNMENT));
    assert(PACKET_SIZE >= sizeof(RR_Message_t));

    pan_sys_x = GET_ROOT()->ProcRoot->MyX;
    pan_sys_y = GET_ROOT()->ProcRoot->MyY;

    InitSem(&pan_comm_req_lock, 1);
    InitSem(&pan_comm_upcall_lock, 1);

    pan_trc_start_upcall = trc_new_event(2999, 0, "start upcall",
					 "start upcall");
    pan_trc_end_upcall   = trc_new_event(2999, 0, "end upcall", "end upcall");


    link_to_seq();

    pan_comm_bcast_seq_start(n_servers);
    pan_comm_bcast_fwd_start();
    pan_comm_bcast_snd_start();

    pan_comm_ucast_start();

    pan_comm_info_set(TIME_SLOT, (int)TimeNowHigh());
}



void
pan_sys_comm_end(void)
{
    long unsigned int t;

    t = (long unsigned int)pan_comm_info_get(TIME_SLOT);
    pan_comm_info_set(TIME_SLOT, (int)((TimeNowHigh() - t) / CLK_TCK_HIGH));

    pan_comm_send_control_msg(STOP_TAG);

    pan_comm_bcast_seq_end();
    pan_comm_bcast_snd_end();
    pan_comm_bcast_fwd_end();

    pan_comm_ucast_end();

    if (pan_sys_link_connected) {
	if (BreakLink(pan_bcast_req_link) != 0) {
	    pan_panic("BreakLink failed");
	}
	if (BreakLink(pan_bcast_data_link) != 0) {
	    pan_panic("BreakLink failed");
	}
    }

    ChangePriority(LOW_PRIORITY);
}


void
pan_sys_frag_print(pan_fragment_p frag)
{
    pan_nsap_p nsap;
    pan_nsap_p pushed_nsap;

    nsap = frag->nsap;
    pushed_nsap = pan_sys_fragment_nsap_look(frag);

    pan_sys_printf("frag 0x%x size %d nsap 0x%x id %d comm hdr size %d frag hdr size %d: pushed nsap-id %d nsap 0x%x\n",
		   frag, frag->size, frag->nsap, frag->nsap->nsap_id,
		   nsap->comm_hdr_size,
		   nsap->type == PAN_NSAP_SMALL ? 0 : nsap->hdr_size + FRAG_HDR_SIZE,
		   pushed_nsap->nsap_id, pushed_nsap);
}
