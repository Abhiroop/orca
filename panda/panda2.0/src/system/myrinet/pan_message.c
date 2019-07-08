/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/*
 * Nowadays, the message invariant runs thus:
 *
 * A panda-created message has an embedded buffer of size PACKET_SIZE.
 * If a push is done that may exceed PACKET_SIZE after header pushes,
 * a new data buffer is malloc'd. The embedded buffer stays alive, but is
 * unused.
 * If the message is created by FM, an embedded buffer of the appropriate
 * size is allocated by FM.
 *
 * RFHH
 */

#include <string.h>
#include <assert.h>

#include "fm.h"

#include "pan_sys_msg.h"		/* Provides a system interface */

#include "pan_global.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_nsap.h"
#include "pan_comm.h"

#include "pan_malloc.h"		/* May use optimized system-level malloc RFHH */
#include "pan_sync.h"		/* Use inline mutexes */





#define RTS_HEADER_SIZE		96	/* Reserve this for headers after
					 * "the big push" */
#define RTS_HDR_ALIGNED		univ_align(RTS_HEADER_SIZE)

#define PACKET_SIZE		((0x1) << LOG_PACKET_SIZE)

#define upround_2power(n, sz)	(((n) + (sz) - 1) & ~((sz) - 1))


static struct pan_mutex lock;	/* protect message cache */
static pan_msg_p msg_cache;	/* caching one is cheaper than malloc/free */


INLINE void
pan_msg_empty(pan_msg_p msg)
{
    if (msg_type(msg) & EMBEDDED_DATA) {
					/* Reuse the embedded data,
					 * irrespective of its size */
    } else if (! (msg_type(msg) & FM_MSG)) {
					/* Reuse the embedded data */
	pan_free(msg_data(msg));
	msg_type(msg) |= EMBEDDED_DATA;
	msg_data(msg) = ((char*)msg) + FM_BUF_ADMIN_SIZE;
	msg_size(msg) = PACKET_SIZE;
    }

    msg_offset(msg) = 0;
}
 


void
pan_sys_msg_start(void)
{
    if (! aligned(PACKET_SIZE, UNIVERSAL_ALIGNMENT)) {
	pan_panic("! aligned(PACKET_SIZE, UNIVERSAL_ALIGNMENT)");
    }
    if (sizeof(struct pan_msg) > FM_BUF_ADMIN_SIZE) {
	pan_panic("sizeof(struct pan_message) > FM_BUF_ADMIN_SIZE");
    }
    if (FM_BUF_ADMIN_SIZE != univ_align(FM_BUF_ADMIN_SIZE)) {
        pan_panic("FM_BUF_ADMIN_SIZE != univ_align(FM_BUF_ADMIN_SIZE)");
    }
    pan_mutex_init(&lock);
}


void
pan_sys_msg_end(void)
{
}



pan_msg_p
pan_msg_create(void)
{
    pan_msg_p msg;

    /* test and test-and-set */
    if (msg_cache) {
	pan_mutex_lock( &lock);
	if (msg_cache) {
	    msg = msg_cache;
	    msg_cache = 0;
	    pan_mutex_unlock( &lock);
    	    msg_offset(msg) = 0;
#if MAX_RELEASE_FUNCS > 0
            msg->pm_nr_rel = 0;
#endif
	    return msg;
	}
	pan_mutex_unlock( &lock);
    }
    msg = pan_malloc(FM_BUF_ADMIN_SIZE + PACKET_SIZE);

    msg_type(msg) = EMBEDDED_DATA;
    msg_data(msg) = ((char*)msg) + FM_BUF_ADMIN_SIZE;
    msg_size(msg) = PACKET_SIZE;
    msg_offset(msg) = 0;
    
#if MAX_RELEASE_FUNCS > 0
    msg->pm_nr_rel = 0;
#endif

    return msg;
}


void
pan_msg_clear(pan_msg_p msg)
{
#if MAX_RELEASE_FUNCS > 0
    int n;

    if (msg->pm_nr_rel > 0) {
	n = --msg->pm_nr_rel;
	msg->pm_func[n](msg, msg->pm_arg[n]);
    }else{
	if (! (msg_type(msg) & EMBEDDED_DATA)) {
	    pan_free(msg_data(msg));
	}
	if (msg_type(msg) & FM_MSG) {
	    FM_free_buf(msg);
	} else {
	    pan_free(msg);
	}
    }
#else
    if (! (msg_type(msg) & FM_MSG) && msg_cache == 0) {
	/* Only cache messages that were created locally. The other
	   ones might be (too) small. */
	pan_mutex_lock( &lock);
        if (msg_cache == 0) {
	    msg_cache = msg;
	    msg_offset(msg) = 0;
	    pan_mutex_unlock( &lock);
	    return;
	}
	pan_mutex_unlock( &lock);
    }

    if (! (msg_type(msg) & EMBEDDED_DATA)) {
	pan_free(msg_data(msg));
    }
    if (msg_type(msg) & FM_MSG) {
	FM_free_buf((struct FM_buffer*)msg);
    } else {
	pan_free(msg);
    }
#endif
}



void *
pan_msg_push(pan_msg_p msg, int length, int align)
{
    int            size;
    int            clear;
    int            cur_offset;
    void	  *buf;
#ifndef NDEBUG
    int            wanted_size = msg_offset(msg) + length;
#endif

    length = univ_align(length);
    cur_offset = msg_offset(msg);

    /* Koen: optimization below fails for servers where a request message is
     * reused to send the reply. In this case the XXX_HEADER_SIZE will never
     * fit into the buffer because it overestimates the true header size.
     * Hack: only optimize for messages that are created locally.
     * [drawback: if the reply data is larger than the request by a small
     * amount ( < header size), then a memcpy() has to be done iso malloc().]
     */
    if (cur_offset == 0 && !(msg_type(msg) & FM_MSG)) {
				/* First push: reserve some space for later
				 * header pushes (RTS_HDR_ALIGNED).
				 * This should save buffer reallocations.
				 */

	clear = MAX_COMM_HDR_SIZE + RTS_HDR_ALIGNED;
    } else {
	clear = MAX_COMM_HDR_SIZE;
    }
    size  = cur_offset + length + clear;

    if (size > msg_size(msg)) {
				/* ALWAYS reserve space for later header pushes.
				 */
        clear = MAX_COMM_HDR_SIZE + RTS_HDR_ALIGNED;
    	size  = cur_offset + length + clear;
	size  = upround_2power(size, PACKET_SIZE);
        if (msg_type(msg) & EMBEDDED_DATA) {
				/* Ruthlessly overwrite msg_data(msg),
				 * thereby ignore the preallocated buffer
				 * space. This is wasted, then :-(
				 */

    	    buf = pan_malloc(size);
	    if ( cur_offset > 0) {
	        memcpy( buf, msg_data(msg), (size_t) cur_offset);
	    }
	    msg_data(msg) = buf;
    	    msg_type(msg) &= ~EMBEDDED_DATA;
        } else {
#ifndef NDEBUG
	    printf("%2d: pan_msg_push must realloc the msg data buffer\n",
		   pan_my_pid());
#endif
	    buf = pan_malloc(size);
	    memcpy(buf, msg_data(msg), cur_offset);
	    pan_free(msg_data(msg));
	    msg_data(msg) = buf;
        }
        msg_size(msg) = size;
    }

    msg_offset(msg) += length;
    assert(aligned(msg_offset(msg), UNIVERSAL_ALIGNMENT));
    assert(msg_size(msg) >= wanted_size);

    return msg_data(msg) + cur_offset;
}



void *
pan_msg_pop(pan_msg_p msg, int length, int align)
{
    if (msg_offset(msg) == 0 && length != 0){
	return NULL;
    }
    
    if (align > UNIVERSAL_ALIGNMENT){
	/* may not be properly aligned */
	pan_panic("alignment");
    }

    length = univ_align(length);

    assert(msg_offset(msg) >= length);

    msg_offset(msg) -= length;
    assert(aligned(msg_offset(msg), UNIVERSAL_ALIGNMENT));
    
    return msg_data(msg) + msg_offset(msg);
}


void *
pan_msg_look(pan_msg_p msg, int length, int align)
{
    int            offset;

    offset = msg_offset(msg);

    if (offset == 0){
	return NULL;
    }
    
    if (align > UNIVERSAL_ALIGNMENT){
	/* may not be properly aligned */
	pan_panic("alignment");
    }

    length = univ_align(length);

    assert(offset >= length);

    return msg_data(msg) + offset - length;
}


void
pan_msg_copy(pan_msg_p msg, pan_msg_p copy)
{
    if (msg_size(copy) < msg_offset(msg) + MAX_COMM_HDR_SIZE) {
	if (msg_size(copy) > 0 && ! (msg_type(copy) & EMBEDDED_DATA)) {
	    pan_free(msg_data(copy));
	} else {
	    msg_type(copy) &= ~EMBEDDED_DATA;
	}
	msg_data(copy) = pan_malloc(msg_size(msg));
	msg_size(copy) = msg_size(msg);
    }

    memcpy(msg_data(copy), msg_data(msg), msg_offset(msg));

    msg_offset(copy) = msg_offset(msg);
    assert(aligned(msg_offset(copy), UNIVERSAL_ALIGNMENT));
}


void
pan_msg_release_push(pan_msg_p msg, pan_msg_release_f func, void *arg)
{
#if MAX_RELEASE_FUNCS > 0
    int n;

    n = msg->pm_nr_rel;
    if (n == MAX_RELEASE_FUNCS){
	pan_panic("release function slots occupied");
    }

    msg->pm_func[n] = func;
    msg->pm_arg[n]  = arg;

    msg->pm_nr_rel++;
#else
    pan_panic("release functions not supported");
#endif
}


void
pan_msg_release_pop(pan_msg_p msg)
{
#if MAX_RELEASE_FUNCS > 0
    assert(msg->pm_nr_rel > 0);
    --msg->pm_nr_rel;
#else
    pan_panic("release functions not supported");
#endif
}
