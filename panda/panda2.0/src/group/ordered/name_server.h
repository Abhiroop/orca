#ifndef __GROUP_NAME_SERVER_H__
#define __GROUP_NAME_SERVER_H__

#include "pan_sys.h"

#include "global.h"
#include "header.h"


void       pan_ns_start(void);

void       pan_ns_handle_end(grp_hdr_p hdr);

void       pan_ns_end(void);

void       pan_ns_handle_register(grp_hdr_p hdr, pan_fragment_p frag);

group_id_t pan_ns_register(char *name);

void       pan_ns_handle_group_id(grp_hdr_p hdr);

#endif
