/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */


#include "pan_sys.h"

#include "pan_group.h"

#include "grp_types.h"

#include "grp_send.ci"
#include "grp_rcve.ci"

#include "grp_send_rcve.h"





void
pan_grp_init_send_rcve(pan_group_p g, void (*rcve)(pan_msg_p msg))
{
    pan_grp_init_send(g);
    pan_grp_init_rcve(g, rcve);
}


void
pan_grp_clear_send_rcve(pan_group_p g)
{
    pan_grp_clear_rcve(g);
    pan_grp_clear_send(g);
}


void
pan_grp_send_rcve_start(void)
{
    pan_grp_send_start();
    pan_grp_rcve_start();
}



void
pan_grp_send_rcve_end(void)
{
    pan_grp_send_end();
    pan_grp_rcve_end();
}
