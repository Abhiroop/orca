#ifndef __RTS_INIT_H__
#define __RTS_INIT_H__

/* These functions take care of initialising and terminating all
 * RTS modules.
 */

extern void rts_init(int me, int nr_platforms, int trace_level);
extern void rts_end(void);

#endif
