#ifndef _SYS_AMOEBA_GLOCAL_
#define _SYS_AMOEBA_GLOCAL_

#include <thread.h>

struct pan_key{
    int key;
};

extern void pan_sys_glocal_start(void);
extern void pan_sys_glocal_end(void);

#endif
