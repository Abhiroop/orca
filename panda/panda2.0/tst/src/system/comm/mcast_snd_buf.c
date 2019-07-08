#include <assert.h>

#include "pan_sys.h"

#include "mcast_hdr.h"
#include "mcast_snd_buf.h"

#ifndef FALSE
#  define FALSE	0
#endif
#ifndef TRUE
#  define TRUE	1
#endif

/*
 * buffer to store/retrieve msgs for sending of later fragments
 */





#define SND_INDEX(buf, n)	(n & ((buf)->size - 1))


struct snd_buf {
    frag_hdr_p      hdr;
    pan_msg_p      *msg;
    int             size;
    int             next;
    int             last;
    pan_mutex_p     lock;
    pan_cond_p      non_empty;
    pan_cond_p      non_full;
    int             done;
};


snd_buf_p
snd_buf_create(pan_mutex_p lock, int size)
{
    snd_buf_p buf;
    
    buf = pan_malloc(sizeof(snd_buf_t));
    
				/* Size must be power of 2 */
    for (buf->size = 1; buf->size < size; buf->size *= 2);

    buf->hdr       = pan_malloc(buf->size * sizeof(frag_hdr_t));
    buf->msg       = pan_malloc(buf->size * sizeof(pan_msg_p));

    buf->next      = 0;
    buf->last      = 0;
    buf->lock      = lock;
    buf->non_empty = pan_cond_create(buf->lock);
    buf->non_full  = pan_cond_create(buf->lock);
    buf->done      = FALSE;

    return buf;
}


void
snd_buf_clear(snd_buf_p buf)
{
    pan_cond_clear(buf->non_empty);
    pan_cond_clear(buf->non_full);
    pan_free(buf->hdr);
    pan_free(buf->msg);
    pan_free(buf);
}


void
snd_buf_put(snd_buf_p buf, frag_hdr_p hdr, pan_msg_p msg)
{
    int idx;
    int     must_wait = FALSE;

    while (buf->next - buf->last == buf->size) {
	must_wait = TRUE;
	pan_cond_wait(buf->non_full);
    }
    if (must_wait) {
	pan_cond_signal(buf->non_full);
    }

    idx = SND_INDEX(buf, buf->next);
    buf->hdr[idx] = *hdr;
    buf->msg[idx] = msg;
    ++buf->next;

    pan_cond_signal(buf->non_empty);
}


int
x_snd_buf_get(snd_buf_p buf, frag_hdr_p hdr, pan_msg_p *msg)
{
    int idx;

    pan_mutex_lock(buf->lock);
    while (buf->next == buf->last && ! buf->done) {
	pan_cond_wait(buf->non_empty);
	if (buf->done) {
	    pan_mutex_unlock(buf->lock);
	    return FALSE;
	}
    }

    idx = SND_INDEX(buf, buf->last);
    *hdr = buf->hdr[idx];
    *msg = buf->msg[idx];
#ifndef NDEBUG
    buf->msg[idx] = NULL;
#endif
    ++buf->last;

    pan_mutex_unlock(buf->lock);

    return TRUE;
}


void
snd_poison(snd_buf_p buf)
{
    pan_mutex_lock(buf->lock);
    buf->done = TRUE;
    pan_cond_signal(buf->non_empty);
    pan_mutex_unlock(buf->lock);
}
