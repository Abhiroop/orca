#ifndef _SYS_ACTMSG_GLOCAL_
#define _SYS_ACTMSG_GLOCAL_

#include <ot.h>

struct pan_key{
    int k_id;
};

extern void pan_sys_glocal_start(void);
extern void pan_sys_glocal_end(void);

#endif /* _SYS_ACTMSG_GLOCAL_ */
