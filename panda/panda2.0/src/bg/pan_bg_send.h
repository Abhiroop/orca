#ifndef __BIGGRP_SEND_MAP_H__
#define __BIGGRP_SEND_MAP_H__

extern void send_start(void);
extern void send_end(void);
extern void send_group(pan_msg_p message);
extern void send_signal(int src, int dest);
extern void pan_bg_send_tick(int finish);

#endif
