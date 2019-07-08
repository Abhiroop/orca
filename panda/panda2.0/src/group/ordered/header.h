#ifndef __GROUP_HEADER_H__
#define __GROUP_HEADER_H__

#include "pan_sys.h"

#include "global.h"
#include "grp.h"


typedef struct grp_hdr grp_hdr_t, *grp_hdr_p;


typedef enum byte_order {
    BIG_ENDIAN,
    LITTLE_ENDIAN
} byte_order_t, *byte_order_p;


typedef enum type {
    G_REGISTER,     /* register a group name (to name server) */
    G_GROUPID,      /* hand out a group id (from name server) */
    G_DONE,         /* inform about shutdown (to name server) */
    G_SEND,         /* pure data message                      */
    G_JOIN,         /* sent when joining a group              */
    G_LEAVE         /* sent when leaving a group              */
} type_t, *type_p;


struct grp_hdr {
    unsigned char type;		/* actually, a type_t */
    short int     gid;		/* actually, a group_id_t */
    short int     ticket;
    short int     sender;
#ifdef HETEROGENEOUS
    byte_order_t  order;
#endif
};


void       pan_hdr_start(void);

void       pan_hdr_end(void);

#endif
