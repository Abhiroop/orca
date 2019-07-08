#ifndef __RTS_SYNC_H__
#define __RTS_SYNC_H__

void rts_sync_init(void);
void rts_sync_end(void);

unsigned short rts_sync_get(void);
void           rts_sync_signal(unsigned short id);
void           rts_sync_wait(unsigned short id);

#endif /* __RTS_SYNC_H__ */
