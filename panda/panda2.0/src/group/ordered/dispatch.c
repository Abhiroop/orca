#include "pan_sys.h"

#include "dispatch.h"
#include "global.h"
#include "grp.h"
#include "join_leave.h"
/* #include "name_server.h" */
#include "send.h"


/* Author: Raoul Bhoedjang, October 1993
 *
 * Dispatch:
 *   The nsaps for fragment and small group msgs are created/cleared.
 *   Dispatch strips incoming group messages from their header
 *   and uses the type field in the header to determine to
 *   what module the message should be passed on.
 */


static void
frag_switch(pan_fragment_p frgm)
{
    grp_hdr_p hdr = pan_fragment_header(frgm);

    switch (hdr->type) {
    case G_JOIN:
	pan_grp_handle_join(hdr, frgm);
	break;
    case G_LEAVE:
	pan_grp_handle_leave(hdr, frgm);
	break;
    case G_SEND:
	pan_grp_handle_send(hdr, frgm);
	break;
    default:
	pan_panic("frag_switch: illegal type");
    }
}


static void
small_switch(void *v_hdr)
{
    grp_hdr_p hdr = v_hdr;

    switch (hdr->type) {
    /*
    case G_SEND:
	pan_grp_handle_send(hdr, NULL);
	break;
    */
    default:
	pan_panic("group_switch: illegal type");
    }
}


void
pan_grp_disp_start(void)
{
    pan_grp_nsap = pan_nsap_create();
    pan_nsap_fragment(pan_grp_nsap, frag_switch, sizeof(grp_hdr_t),
			PAN_NSAP_UNICAST | PAN_NSAP_MULTICAST);
}


void
pan_grp_disp_end(void)
{
    pan_nsap_clear(pan_grp_nsap);
}
