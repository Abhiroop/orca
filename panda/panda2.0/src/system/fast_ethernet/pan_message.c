/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <string.h>
#include <stddef.h>
#include <assert.h>

#include "pan_sys.h"		/* Provides a system interface */

#include "pan_global.h"
#include "pan_message.h"
#include "pan_error.h"
#include "pan_buffer.ci"
#include "pan_fragment.ci"
#include "pan_nsap.h"
#include "pan_comm.h"
#include "pan_sync.h"		/* May use optimized system-level sync RFHH */
#include "pan_malloc.h"		/* May use optimized system-level malloc RFHH */


#define RTS_HEADER_SIZE	96		/* Reserve this for headers after
					* "the big push" */

#define RTS_HDR_ALIGNED	univ_align(RTS_HEADER_SIZE)

#define MSG_STRUCT_SIZE	univ_align(sizeof(struct pan_msg))
#define BUF_STRUCT_SIZE	univ_align(sizeof(pan_sys_buffer_t))



static pan_msg_p msg_cache;
static struct pan_mutex lock;	/* protect message cache */


void
pan_sys_msg_start(void)
{
    if (UNIVERSAL_ALIGNMENT < 4) {
	pan_panic("Need alignment >= 4 for hdr packing\n");
    }
    pan_sys_buffer_start();
    pan_sys_fragment_start();

    pan_mutex_init(&lock);
}


void
pan_sys_msg_end(void)
{
    pan_sys_fragment_end();
    pan_sys_buffer_end();
}



static pan_msg_p
pan_msg_do_create(void)
{
    pan_msg_p msg;

    msg = pan_malloc(MSG_STRUCT_SIZE + BUF_STRUCT_SIZE + PACKET_SIZE);

    fragment_embedded(&msg->pm_fragment) = 1;

#if MAX_RELEASE_FUNCS > 0
    msg->pm_nr_rel = 0;
#endif

    return msg;
}


static void
init_embedded_buffer(pan_msg_p msg)
{
    pan_sys_buffer_p buf;

    buf = (pan_sys_buffer_p)(((char *)msg) + MSG_STRUCT_SIZE);
    buffer_offset(buf) = 0;
    buffer_size(buf)   = PACKET_SIZE;

    msg->pm_buffer = buf;
    msg->pm_type   = MSG_EMBEDDED;
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
	} else {
	    pan_mutex_unlock( &lock);
            msg = pan_msg_do_create();
	}
    } else {
        msg = pan_msg_do_create();
    }

    init_embedded_buffer(msg);

#ifndef NDEBUG
    {
    pan_fragment_p frag = &msg->pm_fragment;
    fragment_buffer(frag)   = NULL;
    fragment_data(frag)     = NULL;
    fragment_size(frag)     = -1;
    }
#endif

    return msg;
}


static void
pan_msg_do_clear(pan_msg_p msg)
{
    if (! (msg->pm_type & MSG_EMBEDDED)) {
	pan_sys_buffer_clear(msg->pm_buffer);
    }
    if (msg_cache == 0) {
	pan_mutex_lock( &lock);
        if (msg_cache == 0) {
	    msg_cache = msg;
	    pan_mutex_unlock( &lock);
	    return;
	}
	pan_mutex_unlock( &lock);
    }

    pan_free(msg);
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
	pan_msg_do_clear(msg);
    }
#else
    pan_msg_do_clear(msg);
#endif
}



void
pan_msg_empty(pan_msg_p msg)
{
    if (! (msg->pm_type & MSG_EMBEDDED) &&
	buffer_size(msg->pm_buffer) < PACKET_SIZE) {
	pan_sys_buffer_clear(msg->pm_buffer);
	init_embedded_buffer(msg);
    } else {
	buffer_offset(msg->pm_buffer) = 0;
    }
}



void *
pan_msg_push(pan_msg_p msg, int length, int align)
{
    int              size;
    int              cur_offset;
    int              clear;
    pan_sys_buffer_p buf;
    pan_sys_buffer_p new_buf;

    length = univ_align(length);

    buf = msg->pm_buffer;
    cur_offset = buffer_offset(buf);

					/* First push? Reserve extra header
					 * space, unless the current buffer
					 * is created by the network.
					 */
    if (cur_offset == 0 &&			/* emptied msg */
	! (buffer_type(buf) & BUF_FM_BUFFER)) { /* no network buf */
	clear = MAX_TOTAL_HDR_SIZE + RTS_HDR_ALIGNED;
    } else {
	clear = MAX_TOTAL_HDR_SIZE;
    }

    size = cur_offset + length + clear;

    if (size > buffer_size(buf)) {
					/* Acquire a new buffer */
					/* ALWAYS reserve space for later
					 * header pushes */
#ifndef NDEBUG
	if (cur_offset > 0) {
	    printf("%2d: msg_push reallocs buffer (need %d (already %d data %d clear %d) have %d)\n",
	    pan_my_pid(), size, cur_offset, length, clear, buffer_size(buf));
	}
#endif
	clear = MAX_TOTAL_HDR_SIZE + RTS_HDR_ALIGNED;
	size = cur_offset + length + clear;
	new_buf = pan_sys_buffer_create(size);
	memcpy(buffer_data(new_buf), buffer_data(buf), cur_offset);
	buffer_offset(new_buf) = cur_offset;
	if (msg->pm_type & MSG_EMBEDDED) {
					/* Just leave the embedded data
					 * chunk -- pity it's unused */
	    msg->pm_type &= ~MSG_EMBEDDED;
	} else {
	    pan_sys_buffer_clear(buf);
	}
	buf = new_buf;
	msg->pm_buffer = buf;
    }

    return pan_sys_buffer_push(buf, length);
}


void *
pan_msg_pop(pan_msg_p msg, int length, int align)
{
    return pan_sys_buffer_pop(msg->pm_buffer, length, align);
}


void *
pan_msg_look(pan_msg_p msg, int length, int align)
{
    return pan_sys_buffer_look(msg->pm_buffer, length, align);
}

static void
resize_buffer(pan_msg_p msg, int size)
{
    size = univ_align(size) + MAX_TOTAL_HDR_SIZE;

    if (buffer_size(msg->pm_buffer) < size) {
	if (msg->pm_type & MSG_EMBEDDED) {
	    msg->pm_type &= ~MSG_EMBEDDED;
	} else {
	    pan_sys_buffer_clear(msg->pm_buffer);
	}
	msg->pm_buffer = pan_sys_buffer_create(size);
    }
}


void
pan_msg_copy(pan_msg_p msg, pan_msg_p copy)
{
    resize_buffer(copy, buffer_offset(msg->pm_buffer));
    pan_sys_buffer_copy(msg->pm_buffer, copy->pm_buffer);
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


void
pan_msg_assemble(pan_msg_p msg, pan_fragment_p fragment, int preserve)
{
    pan_sys_buffer_p buf;
    frag_hdr_p       hdr;
    int              src_size;
    unsigned int     flags;

    hdr = fragment_frag_hdr(fragment);
    buf = msg->pm_buffer;

    flags = frag_hdr_get_flags(hdr);

    if (flags & PAN_FRAGMENT_FIRST) {

	assert(buffer_offset(msg->pm_buffer) == 0);
	assert(buffer_data(fragment_buffer(fragment)) ==
		fragment_data(fragment));

	src_size = frag_hdr_get_size(hdr);
	assert(aligned(src_size, UNIVERSAL_ALIGNMENT));

	resize_buffer(msg, src_size);

	if (fragment_embedded(fragment) || preserve) {
	    pan_sys_buffer_copy(fragment_buffer(fragment), msg->pm_buffer);
	} else {

	    if (msg->pm_type & MSG_EMBEDDED) {
				/* After resize still embedded: this means the
				 * fragment is a singleton _and_ it fits in */
		assert(src_size + MAX_TOTAL_HDR_SIZE <= PACKET_SIZE);
		assert(flags & PAN_FRAGMENT_LAST);

		msg->pm_type &= ~MSG_EMBEDDED;
		msg->pm_buffer = fragment_buffer(fragment);
	    } else {
		pan_sys_buffer_copy(fragment_buffer(fragment), msg->pm_buffer);
		pan_sys_buffer_clear(fragment_buffer(fragment));
	    }

	    fragment_buffer(fragment) = NULL;
	}

    } else {
	pan_sys_buffer_append(buf, fragment_buffer(fragment),
			      frag_hdr_get_offset(hdr),
			      fragment_size(fragment));
    }
}


pan_fragment_p
pan_msg_fragment(pan_msg_p msg, pan_nsap_p nsap)
{
    pan_sys_buffer_p  buf;
    char             *buf_data;
    int               buf_size;
    pan_fragment_p    frag;
    int               flags;
    char             *common_hdr;
    frag_hdr_p        frag_hdr;

    buf = msg->pm_buffer;
    buf_data = buffer_data(buf);
    buf_size = buffer_offset(buf);
    assert(aligned(buf_size, UNIVERSAL_ALIGNMENT));

    frag = &msg->pm_fragment;
    assert(fragment_embedded(frag));

    fragment_nsap(frag)     = nsap;
    msg->pm_hdr_size = univ_align(TOTAL_HDR_SIZE(nsap));

    fragment_data(frag)     = buf_data;
    fragment_buffer(frag)   = buf;
    fragment_size(frag)     = MIN(buf_size, PACKET_SIZE - msg->pm_hdr_size);

    common_hdr = fragment_common_hdr(frag);
    frag_hdr   = (frag_hdr_p)(common_hdr + COMMON_HDR_SIZE(nsap));
    assert(aligned(common_hdr + FRAG_HDR_SIZE(nsap) - fragment_data(frag),
		   COMM_HDR_ALIGN(nsap)));

    flags = PAN_FRAGMENT_FIRST;
    if (fragment_size(frag) == buf_size) {	/* Buffer fits in fragment */
	flags |= PAN_FRAGMENT_LAST;
    }else{
	/* Copy the user data that is going to be overwritten to backup */
	memcpy(msg->pm_backup, common_hdr, msg->pm_hdr_size);
    }

				/* Marshall the fragment header in the buffer */
    frag_hdr_set_size(frag_hdr, buf_size);
    frag_hdr_set_flags(frag_hdr, flags);

    return frag;
}


pan_fragment_p
pan_msg_next(pan_msg_p msg)
{
    pan_fragment_p    frag;
    pan_sys_buffer_p  buf;
    int               buf_size;
    int               buf_offset;
    char             *buf_data;
    int               flags = 0;
    char             *common_hdr;
    frag_hdr_p        frag_hdr;
    pan_nsap_p        nsap;

    frag = &msg->pm_fragment;
    assert(fragment_embedded(frag));

    frag_hdr = fragment_frag_hdr(frag);

    if (frag_hdr_get_flags(frag_hdr) & PAN_FRAGMENT_LAST){
				/* Last fragment already handed out */
	return NULL;
    }

    nsap = fragment_nsap(frag);

    buf = msg->pm_buffer;
    buf_offset = (fragment_data(frag) - buffer_data(buf)) + fragment_size(frag);

    buf_data = buffer_data(buf) + buf_offset;
    buf_size = buffer_offset(buf) - buf_offset;
    assert(buf_size >= 0);

				/* Restore original data */
    common_hdr = fragment_common_hdr(frag);
    memcpy(common_hdr, msg->pm_backup, msg->pm_hdr_size);

    fragment_data(frag)     = buf_data;
    fragment_size(frag)     = MIN(buf_size, PACKET_SIZE - msg->pm_hdr_size);

    common_hdr = fragment_common_hdr(frag);
    frag_hdr   = (frag_hdr_p)(common_hdr + COMMON_HDR_SIZE(nsap));
    assert(aligned(common_hdr + FRAG_HDR_SIZE(nsap) - fragment_data(frag),
		   COMM_HDR_ALIGN(nsap)));

    if (fragment_size(frag) == buf_size){
	flags = PAN_FRAGMENT_LAST;
    }else{
	/* Copy the user data that is going to be overwritten to backup */
	memcpy(msg->pm_backup, common_hdr, msg->pm_hdr_size);
    }

				/* Marshall the fragment header in the buffer */
    frag_hdr_set_offset(frag_hdr, buf_offset);
    frag_hdr_set_flags(frag_hdr, flags);

    return frag;
}
