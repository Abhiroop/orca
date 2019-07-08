#ifndef _SYS_ACTMSG_SMALL_
#define _SYS_ACTMSG_SMALL_

extern int PAN_0;
extern int pan_small_handler_0;

#define PAN_SMALL_HANDLER_SIZE  ((char *)(&PAN_0) - (char *)(&pan_small_handler_0))

#define PAN_SMALL_HANDLER(ni)   (void (*)(int a0, int a1, int a2, int a3)) \
                                ( (char *)(&pan_small_handler_0) + \
                                  (ni) * PAN_SMALL_HANDLER_SIZE )

#endif /* _SYS_ACTMSG_SMALL_ */


