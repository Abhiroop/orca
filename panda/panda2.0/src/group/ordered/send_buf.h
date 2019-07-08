#ifndef __MCAST_SND_BUF_H__
#define __MCAST_SND_BUF_H__

#include "pan_sys.h"

#include "header.h"

/*
 * buffer to store/retrieve msgs for sending of later fragments
 */


typedef struct snd_buf snd_buf_t, *snd_buf_p;



snd_buf_p snd_buf_create(pan_mutex_p lock, int size);

void      snd_buf_clear(snd_buf_p buf);

void      snd_buf_put(snd_buf_p buf, grp_hdr_p hdr, pan_msg_p msg);

int       x_snd_buf_get(snd_buf_p buf, grp_hdr_p hdr, pan_msg_p *msg);

void      snd_poison(snd_buf_p buf);


#endif
