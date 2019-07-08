#ifndef __GROUP_SEND_H__
#define __GROUP_SEND_H__

#include "pan_sys.h"

#include "header.h"


void    pan_grp_snd_start(void);

void    pan_grp_snd_end(void);

void    pan_grp_handle_send(grp_hdr_p hdr, pan_fragment_p msg);

#endif
