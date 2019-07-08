/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>

#include "pan_sys.h"

#include "pan_util.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"
#include "trc_event_tp.h"
#include "trc_lib.h"
#include "trc_io.h"
#include "trc_trace.h"


static trc_event_t	START_USR_FLUSH;
static trc_event_t	END_USR_FLUSH;



#define TRC_EVENTS 256
#define DEFAULT_BUF 1024

#define TRC_MAX_LEVEL  USHRT_MAX


static trc_t trc_global_state;



void
trc_start(char *filename, int max_buf_size)
{
    int         p;
    pan_time_p  t = pan_time_create();

    trc_global_state.key = pan_key_create();

    trc_global_state.my_pid = pan_my_pid();
    trc_global_state.n_pids = pan_nr_platforms();

    trc_global_state.thread = pan_calloc(trc_global_state.n_pids + 1 + PANDA_1,
					 sizeof(trc_pid_threads_t));
    ++trc_global_state.thread;		/* Reserve a seat for thread -1 */

    /* Manage indices -1 .. n_pids (incl) */
    for (p = -1; p < trc_global_state.n_pids + PANDA_1; p++) {
	trc_global_state.thread[p].threads = NULL;
	trc_global_state.thread[p].n_threads = 0;
    }
    trc_global_state.n_threads = 0;

    trc_global_state.max_mem_size = max_buf_size;
    trc_global_state.mem_size = 0;

    trc_global_state.level = TRC_MAX_LEVEL;
    trc_global_state.event_list = trc_event_lst_init();
    trc_global_state.out_buf.format = IMPLICIT_SRC | COMPACT_FORMAT;
    trc_global_state.filename = pan_malloc(strlen(filename) + 20);
    sprintf(trc_global_state.filename, "%s-%d.%d",
	    filename, trc_global_state.my_pid, trc_global_state.n_pids);
    trc_global_state.out_buf.stream = fopen(trc_global_state.filename, "a");
    if (trc_global_state.out_buf.stream == NULL) {
	pan_panic("trace file fopen failed");
    }

    trc_global_state.lock = pan_mutex_create();

    /* START_TRACE = trc_new_event(0, sizeof(pan_time_fix_t), "START_TRACE",
     * "(%d,%d)"); */
    /* END_TRACE   = trc_new_event(0, sizeof(pan_time_fix_t), "END_TRACE",
     * "(%d,%d)"); */
    CLOCK_SHIFT = trc_new_event(TRC_MAX_LEVEL, sizeof(pan_time_fix_t),
				"clock_shift", "(%d,%d)");
    FLUSH_BLOCK = trc_new_event(TRC_MAX_LEVEL, sizeof(size_t), "flush_block",
				"%d data bytes");
    START_USR_FLUSH = trc_new_event(TRC_MAX_LEVEL, 0, "start usr flush", "");
    END_USR_FLUSH = trc_new_event(TRC_MAX_LEVEL, 0, "end usr flush", "");

    trc_global_state.t_start_native = pan_time_create();
    pan_time_get(trc_global_state.t_start_native);
    pan_time_t2fix(trc_global_state.t_start_native, &trc_global_state.t_start);

    trc_global_state.t_off = pan_time_fix_zero;
    trc_global_state.t_d_off = pan_time_fix_zero;

    if (! pan_is_bigendian()) {
	trc_global_state.out_buf.format |= LITTLE_ENDIAN;
    }

    trc_global_state.rebind = NULL;

    pan_time_clear(t);
}


/* Assumes trc_global_state.lock is locked when this function is called.
 * The blocks can only be cleared by the owning thread or at shut-down
 * time, when there is only one thread left.
 */
static size_t
trc_flush_thread(trc_thread_info_p thread_info, boolean clear_bufs)
{
    size_t      size;
    trc_lst_p   block;
    trc_lst_p   clr_block;
    FILE       *s;
    int         n;

    size = 0;
    for (block = thread_info->trc_front; block != NULL; block = block->next) {
	block->flush_start = block->flush_end;
	block->flush_end = block->current;
	size += block->flush_end - block->flush_start;
    }
    if (size == 0) {
	return size;
    }

    trc_block_hdr_write(&trc_global_state, size, thread_info->thread_id);
    s = trc_global_state.out_buf.stream;
    for (block = thread_info->trc_front; block != NULL; block = block->next) {
	n = block->flush_end - block->flush_start;
	if (n > 0 && fwrite(block->flush_start, 1, n, s) != n) {
	    pan_panic("fwrite of trace info failed");
	}
	block->flush_start = block->flush_end;
    }

    if (fflush(s) != 0) {
	pan_panic("fflush of trace state info failed");
    }
    if (clear_bufs) {
	trc_global_state.mem_size -= size;
	block = thread_info->trc_front;
	while (block != NULL) {
	    clr_block = block;
	    block = block->next;
	    pan_free(clr_block->buf);
	    pan_free(clr_block);
	}
	thread_info->trc_front = NULL;
	thread_info->trc_tail = NULL;
    }
    return size;
}


/* Concurrent use of extend_buf and trc_flush: lock trc_global_state.lock */
static size_t
extend_buf(trc_thread_info_p thread_info, size_t size)
{
    trc_thread_info_p t;
    trc_lst_p         p;
    size_t            bytes_flushed = 0;

    pan_mutex_lock(trc_global_state.lock);

    if (trc_global_state.mem_size + size > trc_global_state.max_mem_size) {
	t = trc_global_state.thread[trc_global_state.my_pid].threads;
	while (t != NULL) {
	    t->must_flush = TRUE;
	    t = t->next_thread;
	}
    }
    if (thread_info->must_flush) {
	bytes_flushed = trc_flush_thread(thread_info, TRUE);
	thread_info->must_flush = FALSE;
    }
    trc_global_state.mem_size += size;

    /* Buy link for linked list */
    p = pan_malloc(sizeof(trc_lst_t));
    p->buf = pan_malloc(size);
    p->current = p->buf;
    p->flush_start = p->buf;
    p->flush_end = p->buf;
    p->size = size;
    /* Push it on the linked list */
    p->next = NULL;
    if (thread_info->trc_tail == NULL) {
	thread_info->trc_front = p;
	thread_info->trc_tail = p;
    } else {
	thread_info->trc_tail->next = p;
	thread_info->trc_tail = p;
    }

    pan_mutex_unlock(trc_global_state.lock);

    return bytes_flushed;
}


static void
do_trc_flush(boolean clear_blocks)
{
    trc_thread_info_p p;
    pan_time_p        t_off = pan_time_create();
    pan_time_p        t_d_off = pan_time_create();

    if (clear_blocks) {
	trc_global_state.entries = 0;
	p = trc_global_state.thread[trc_global_state.my_pid].threads;
	while (p != NULL) {
	    trc_global_state.entries += p->entries;
	    p = p->next_thread;
	}
    } else {
	trc_global_state.entries = -1;
    }

    pan_clock_get_shift(t_off, t_d_off);
    pan_time_t2fix(t_off, &trc_global_state.t_off);
    pan_time_t2fix(t_d_off, &trc_global_state.t_d_off);

    trc_state_write(&trc_global_state);

    p = trc_global_state.thread[trc_global_state.my_pid].threads;
    while (p != NULL) {
	trc_flush_thread(p, clear_blocks);
	p = p->next_thread;
    }

    pan_time_clear(t_off);
    pan_time_clear(t_d_off);
}


/* Concurrent use of extend_buf and trc_flush: lock trc_global_state.lock */
void
trc_flush(void)
{
    trc_event(START_USR_FLUSH, NULL);
    pan_mutex_lock(trc_global_state.lock);
    do_trc_flush(FALSE);
    pan_mutex_unlock(trc_global_state.lock);
    trc_event(END_USR_FLUSH, NULL);
}


void
trc_end(void)
{
    pan_time_p  t = pan_time_create();
    int         p;

    pan_time_get(t);
    pan_time_t2fix(t, &trc_global_state.t_stop);
    do_trc_flush(TRUE);

    pan_mutex_clear(trc_global_state.lock);

    if (fclose(trc_global_state.out_buf.stream) != 0) {
	pan_panic("%2d: close of trace file %s failed\n",
		  pan_my_pid(), trc_global_state.filename);
    }
 
    pan_free(trc_global_state.filename);
    
    for (p = -1; p < trc_global_state.n_pids + PANDA_1; p++) {
        if (trc_global_state.thread[p].threads != NULL) {
            pan_free(trc_global_state.thread[p].threads);
        }
    }
    
    --trc_global_state.thread;
    pan_free(trc_global_state.thread);

    pan_time_clear(t);
    pan_time_clear(trc_global_state.t_start_native);
}


void
trc_new_thread(int buf_size, char *name)
{
    trc_thread_info_p thread_info;
    trc_thread_info_p scan;

					/* Buy glocal data */
    thread_info = pan_malloc(sizeof(trc_thread_info_t));
    pan_key_setspecific(trc_global_state.key, thread_info);

    if (buf_size == 0) {
	buf_size = DEFAULT_BUF;
    }
    thread_info->t_current = pan_time_create();
    thread_info->t_scratch = pan_time_create();
    pan_time_copy(thread_info->t_current, trc_global_state.t_start_native);
    thread_info->must_flush = FALSE;
    thread_info->trc_front = NULL;
    thread_info->trc_tail = NULL;
    thread_info->entries = 0;
    extend_buf(thread_info, buf_size);
    thread_info->name = pan_malloc(strlen(name) + 1);
    strcpy(thread_info->name, name);

					/* Append to global list */
    pan_mutex_lock(trc_global_state.lock);
    scan = trc_global_state.thread[trc_global_state.my_pid].threads;
    if (scan == NULL) {
	trc_global_state.thread[trc_global_state.my_pid].threads = thread_info;
    } else {
	while (scan->next_thread != NULL) {
	    scan = scan->next_thread;
	}
	scan->next_thread = thread_info;
    }
    thread_info->next_thread = NULL;
    trc_global_state.mem_size += buf_size;
    thread_info->thread_id.my_pid = trc_global_state.my_pid;
    thread_info->thread_id.my_thread = trc_global_state.n_threads;
    ++trc_global_state.n_threads;
    ++trc_global_state.thread[trc_global_state.my_pid].n_threads;
    pan_mutex_unlock(trc_global_state.lock);
}


int
trc_set_level(int level)
{
    int         l;

    if (level < 0 || level > TRC_MAX_LEVEL) {
	return -1;
    }
    pan_mutex_lock(trc_global_state.lock);
    l = trc_global_state.level;
    trc_global_state.level = level;
    pan_mutex_unlock(trc_global_state.lock);
    return l;
}


trc_event_t
trc_new_event(int level, size_t u_size, char *name, char *fmt)
{
    trc_event_t e;

    pan_mutex_lock(trc_global_state.lock);
    if (level < 0) {
	level = 0;
    } else if (level >= TRC_MAX_LEVEL) {
	level = TRC_MAX_LEVEL - 1;
    }
    e = trc_event_lst_add(trc_global_state.event_list, level, u_size, name,
			  fmt);
    pan_mutex_unlock(trc_global_state.lock);
    return e;
}


static size_t
put_trc_event(trc_thread_info_p thread_info, pan_time_diff_t dt, trc_event_t e,
	      size_t usr_size, void *user_info)
/* "Tric" to allow concurrent put_event and flush: use pointer p
 * into the buffer, update buf->current only when all writing is done.
 */
{
    trc_lst_p   l;
    size_t      bytes_flushed = 0;
    char       *p;

    l = thread_info->trc_tail;
    if (l->size - (l->current - l->buf) <
	    usr_size + sizeof(trc_event_t) + sizeof(pan_time_diff_t)) {
	bytes_flushed = extend_buf(thread_info, l->size);
	l = thread_info->trc_tail;
    }
    p = l->current;
    ++thread_info->entries;
    memcpy(p, &e, sizeof(trc_event_t));
    p += sizeof(trc_event_t);
    memcpy(p, &dt, sizeof(pan_time_diff_t));
    p += sizeof(pan_time_diff_t);
    memcpy(p, user_info, usr_size);
    p += usr_size;
    l->current = p;
    return bytes_flushed;
}


void
trc_event(trc_event_t e, void *user_info)
{
    trc_thread_info_p thread_info;
    pan_time_fix_t    now;
    long int          diff;
    size_t      bytes_flushed = 0;
    trc_event_lst_p lst;

    lst = trc_global_state.event_list;
    if (trc_event_lst_level(lst, e) < trc_global_state.level) {
	return;
    }
    thread_info = pan_key_getspecific(trc_global_state.key);
    pan_time_get(thread_info->t_scratch);

    if (!pan_time_sub_fix_small(thread_info->t_scratch, thread_info->t_current,
				&diff)) {
	pan_time_t2fix(thread_info->t_scratch, &now);
	bytes_flushed = put_trc_event(thread_info, 0, CLOCK_SHIFT,
				      trc_event_lst_usr_size(lst, CLOCK_SHIFT),
				      &now);
	pan_time_copy(thread_info->t_current, thread_info->t_scratch);
	diff = 0;
    }
    assert(diff >= 0);
    bytes_flushed += put_trc_event(thread_info, diff, e,
				   trc_event_lst_usr_size(lst, e), user_info);

    pan_time_copy(thread_info->t_current, thread_info->t_scratch);
    if (bytes_flushed > 0) {
	trc_event(FLUSH_BLOCK, &bytes_flushed);
    }
}
