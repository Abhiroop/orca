#ifndef __PAN_MP_REL_MSG_H__
#define __PAN_MP_REL_MSG_H__


#include "pan_sys_msg.h"


#ifndef STATIC_CI
#  define STATIC_CI	static
#endif


typedef struct MP_HDR	mp_hdr_t, *mp_hdr_p;

struct MP_HDR {
    short int		ticket;
};


#ifndef STATIC_CI

mp_hdr_p mp_hdr_push(pan_msg_p msg);
mp_hdr_p mp_hdr_pop(pan_msg_p msg);

#endif


#endif
