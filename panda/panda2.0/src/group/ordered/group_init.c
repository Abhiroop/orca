#include "pan_sys.h"

#include "dispatch.h"
#include "group_tab.h"
#include "header.h"
#include "join_leave.h"
#include "name_server.h"
#include "send.h"


/* Author: Raoul Bhoedjang, October 1993
 *
 * Group initialisation:
 *   initialises all modules in the group layer.
 */

void
pan_group_init(void)
{
    pan_grp_global_start();
    pan_hdr_start();
    pan_ns_start();
    pan_grp_snd_start();
    pan_grp_jl_start();
    pan_grp_disp_start();
}


void
pan_group_end(void)
{
    pan_grp_jl_end();
    pan_grp_snd_end();
    pan_ns_end();
    pan_grp_disp_end();
    pan_hdr_end();
    pan_grp_global_end();
}
