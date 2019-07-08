#ifndef __BIGGRP_REC_MAP_H__
#define __BIGGRP_REC_MAP_H__

#include "pan_bg.h"
#include "pan_bg_group.h"

extern void rec_start(void);
extern void rec_end(void);
extern void rec_handle(pan_fragment_p fragment, pan_bg_hdr_p header,
		       int flags);
void pan_bg_rec_fragment(pan_fragment_p fragment, pan_bg_hdr_p header);
void pan_bg_orderer(pan_fragment_p fragment);
extern void pan_bg_rec_tick(int finish);

#endif
