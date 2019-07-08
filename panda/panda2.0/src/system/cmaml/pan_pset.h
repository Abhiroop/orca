#ifndef _SYS_GENERIC_PSET_
#define _SYS_GENERIC_PSET_

struct pan_pset{
    unsigned *mask;
};

extern void pan_sys_pset_start(void);
extern void pan_sys_pset_end(void);

#endif
