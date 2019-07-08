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
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <math.h>
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
#include "trc_bind_op.h"


#define TRC_EVENTS 256


#define OUT_BUF_SIZE 1048576





typedef struct PLATF_LST_T platf_lst_t, *platf_lst_p;

struct PLATF_LST_T {
    int           platf;
    platf_lst_p   next;
    trc_event_descr_p e;
};


typedef struct THREAD_LST_T thread_lst_t, *thread_lst_p;

struct THREAD_LST_T {
    trc_thread_info_p  thread;
    thread_lst_p       next;
    long int           file_pos;
    char              *buf;
    char              *current;
    size_t             size;
    trc_event_descr_p  e;
};






/* -- Time ordered list of event descriptors per platform. */

static void
platf_lst_assert(platf_lst_p p)
{
    pan_time_fix_p t;

    if (p != NULL) {
	t = &p->e->t;
	p = p->next;
    }
    while (p != NULL) {
	assert(pan_time_fix_cmp(t, &p->e->t) <= 0);
	p = p->next;
    }
}



static void
platf_lst_insert(platf_lst_p *front, platf_lst_p item)
{
    platf_lst_p    scan;
    platf_lst_p    prev;
    pan_time_fix_p t;

    if (*front == NULL) {
        item->next = NULL;
        *front = item;
    } else {
        prev = NULL;
        scan = *front;
	t = &item->e->t;
        while (scan != NULL && pan_time_fix_cmp(t, &scan->e->t) > 0) {
            prev = scan;
            scan = scan->next;
        }
        if (prev == NULL) {
            item->next = *front;
            *front = item;
        } else {
            item->next = scan;
            prev->next = item;
        }
    }
}



static platf_lst_p
platf_lst_deq(platf_lst_p *front)
{
    platf_lst_p old_front;

    if (*front == NULL) {
        return NULL;
    }
    old_front = *front;
    *front = old_front->next;
    return old_front;
}

/* -- end of Time ordered list of event descriptors per platform. */


/* -- Time ordered list of event descriptors per thread. */



static void
thread_lst_insert(thread_lst_p *front, thread_lst_p item)
{
    thread_lst_p   scan;
    thread_lst_p   prev;
    pan_time_fix_p t;

    if (*front == NULL) {
        item->next = NULL;
        *front = item;
    } else {
        prev = NULL;
        scan = *front;
	t = &item->e->t;
        while (scan != NULL && pan_time_fix_cmp(t, &scan->e->t) > 0) {
            prev = scan;
            scan = scan->next;
        }
        if (prev == NULL) {
            item->next = *front;
            *front = item;
        } else {
            item->next = scan;
            prev->next = item;
        }
    }
}



static thread_lst_p
thread_lst_deq(thread_lst_p *front)
{
    thread_lst_p old_front;

    if (*front == NULL) {
        return NULL;
    }
    old_front = *front;
    *front = old_front->next;
    return old_front;
}


/* -- end of Time ordered list of event descriptors per thread. */


static trc_event_descr_p
thr_get_next_event(trc_p trace_state, thread_lst_p *thread_lst,
		   trc_event_descr_p e_free)
{
    trc_event_descr_p e;
    thread_lst_p   nxt;
    boolean        is_clockshift;

    do {
        nxt = thread_lst_deq(thread_lst);

        if (nxt == NULL) {
            return NULL;
        }

        e = nxt->e;
	nxt->e = e_free;
	assert(e->type == CLOCK_SHIFT ||
		pan_time_fix_cmp(&nxt->thread->t_fix_current, &e->t) <= 0);
        nxt->thread->t_fix_current = e->t;

        is_clockshift = (e->type == CLOCK_SHIFT);
        if (is_clockshift) {
	    e_free = e;
        }

        if (nxt->current - nxt->buf == nxt->size) {
	    pan_free(nxt->buf);
            nxt->buf = trc_block_thread_read(trace_state,
                                             nxt->thread->thread_id,
                                             &nxt->size, &nxt->file_pos);
            nxt->current = nxt->buf;
        }

        if (nxt->buf != NULL) {
            nxt->current += trc_event_get(nxt->current, trace_state, nxt->e);
            if (IS_IMPLICIT_SRC(trace_state->in_buf.format)) {
		if (nxt->e->type == CLOCK_SHIFT) {
		    assert(nxt->e->usr_size == sizeof(pan_time_fix_t));
		    memcpy(&nxt->e->t, nxt->e->usr, nxt->e->usr_size);
		} else {
		    pan_time_fix_add(&nxt->e->t, &nxt->thread->t_fix_current);
		}
                nxt->e->thread_id = nxt->thread->thread_id;
            }
            thread_lst_insert(thread_lst, nxt);
        } else {
	    trc_event_descr_clear(nxt->e);
	    pan_free(nxt->e);
	    pan_free(nxt);
	}
    } while (is_clockshift);

    return e;
}





static trc_event_descr_p
platf_next_ev(trc_p h, thread_lst_p *thread_lst, trc_event_descr_p e_free)
{
    trc_event_descr_p e = NULL;
    trc_lst_p         buf;
    trc_block_hdr_t   hdr;

    if (IS_THREADS_MERGED(h->in_buf.format)) {
	buf = &h->in_buf;
	if (buf->current - buf->buf == buf->size) {
	    if (buf->buf != NULL) {
		pan_free(buf->buf);
	    }
	    buf->buf     = trc_block_read(h, &hdr);
	    if (buf->buf == NULL) {
		return NULL;
	    }
	    buf->size    = hdr.size;
	    buf->current = buf->buf;
	}
	e = e_free;
	buf->current += trc_event_get(buf->current, h, e);
	assert(e->type != CLOCK_SHIFT);
    } else {
	e = thr_get_next_event(h, thread_lst, e_free);
    }
    if (e != NULL) {
	assert(pan_time_fix_cmp(&e->t, &h->t_start) >= 0);
	pan_time_fix_sub(&e->t, &h->t_off);
	e->type = trc_event_lst_extern_id(h->event_list, e->type);
    } else {
	trc_event_descr_clear(e_free);
	pan_free(e_free);
    }
    return e;
}




static thread_lst_p
init_thread_lst(trc_p trace_state, trc_thread_info_p p, int *entries,
		pan_time_fix_t t_start)
{
    thread_lst_p   lst;
    thread_lst_p   front = NULL;

    *entries = 0;
    for (; p != NULL; p = p->next_thread) {
        lst = pan_malloc(sizeof(thread_lst_t));
        lst->thread   = p;
        lst->file_pos = 0;
        lst->buf      = trc_block_thread_read(trace_state, p->thread_id,
						&lst->size, &lst->file_pos);
        lst->current  = lst->buf;
        *entries += p->entries;

        if (lst->current - lst->buf < lst->size) {
            lst->thread->t_fix_current = t_start;
            lst->e = pan_malloc(sizeof(trc_event_descr_t));
	    trc_event_descr_init(lst->e);
            lst->current += trc_event_get(lst->current, trace_state, lst->e);
            if (IS_IMPLICIT_SRC(trace_state->in_buf.format)) {
		if (lst->e->type == CLOCK_SHIFT) {
		    assert(lst->e->usr_size == sizeof(pan_time_fix_t));
		    memcpy(&lst->e->t, lst->e->usr, lst->e->usr_size);
		} else {
		    pan_time_fix_add(&lst->e->t, &t_start);
		}
                lst->e->thread_id = lst->thread->thread_id;
            }
            thread_lst_insert(&front, lst);
        } else {
	    pan_free(lst);
	}
    }

    return front;
}




static void
init_platform_buf(trc_p h, platf_lst_p *front, thread_lst_p *thread_lst,
		  int pid, pan_time_fix_t t_start)
{
    platf_lst_p     lst;
    trc_lst_p       buf;
    int             entries;
    trc_event_descr_p e;

    lst = pan_malloc(sizeof(platf_lst_t));
    lst->platf = pid;
    if (IS_THREADS_MERGED(h->in_buf.format)) {
	buf = &h->in_buf;
	buf->buf = NULL;
	buf->current = NULL;
	buf->size = 0;
    } else {
	*thread_lst = init_thread_lst(h, h->thread[h->my_pid].threads, &entries,
				      h->t_start);
    }
    e = pan_malloc(sizeof(trc_event_descr_t));
    trc_event_descr_init(e);
    lst->e = platf_next_ev(h, thread_lst, e);
    if (lst->e != NULL) {
	platf_lst_insert(front, lst);
    } else {
	pan_free(lst);
    }
}


static trc_event_descr_p
pl_get_next_event(trc_t trace_state[], thread_lst_p thread_lst[], int n_files,
		  platf_lst_p *platf_lst, trc_event_descr_p e_free)
{
    platf_lst_p      nxt_pl;
    trc_p            h;
    trc_event_descr_p   e;

    nxt_pl = platf_lst_deq(platf_lst);
    if (nxt_pl == NULL) {
	trc_event_descr_clear(e_free);
	pan_free(e_free);
	return NULL;
    }
    e = nxt_pl->e;
    h = &trace_state[nxt_pl->platf];
    nxt_pl->e = platf_next_ev(h, &thread_lst[nxt_pl->platf], e_free);
    if (nxt_pl->e != NULL) {
	platf_lst_insert(platf_lst, nxt_pl);
    } else {
	pan_free(nxt_pl);
    }
    return e;
}


static int
merge_platforms(trc_p par_state, trc_t trace_state[], thread_lst_p thread_lst[],
		int n_files, platf_lst_p *platf_lst, boolean write_data,
		int events_per_dot)
{
    int                out_entries;
    trc_event_descr_p  next_event;
    int                pe;
    int                thr;
    int                count_saw = 0;
#ifndef NDEBUG
    pan_time_fix_t     t_prev = { INT_MIN, 0 };
#endif

    if (events_per_dot != INT_MAX) {
	fprintf(stderr, "[");
    }

    out_entries = 0;
    next_event = pan_malloc(sizeof(trc_event_descr_t));
    trc_event_descr_init(next_event);
    while ((next_event = pl_get_next_event(trace_state, thread_lst, n_files,
					   platf_lst, next_event)) != NULL) {
	out_entries++;

	trc_redef_obj_op(par_state->rebind, next_event);

	pe = next_event->thread_id.my_pid;
	thr = next_event->thread_id.my_thread;
	trc_set(par_state->thread[pe].threads[thr].event_tp_set,
		next_event->type);

#ifndef NDEBUG
	assert(pan_time_fix_cmp(&t_prev, &next_event->t) <= 0);
	t_prev = next_event->t;
#endif

	++count_saw;
	if (count_saw >= events_per_dot) {
	    fprintf(stderr, ".");
	    count_saw = 0;
	}

	if (write_data) {
	    trc_write_next_event(par_state, next_event);
	}
    }

    if (events_per_dot != INT_MAX) {
	fprintf(stderr, "]\n");
    }

    return out_entries;
}



static void
unify_events(trc_event_lst_p src, trc_event_lst_p target)
{
    trc_event_t  e;
    trc_event_t  src_e;
    trc_event_t  e_extern;
    char        *name;
    char        *fmt;
    size_t       usr_size;
    int          level;

    e = trc_event_lst_first(target);
    while (e != -1) {
	trc_event_lst_query_extern(target, e, &e_extern, &name, &fmt);
	src_e = trc_event_lst_find(src, name);
	if (src_e == -1) {
	    trc_event_lst_query(target, e, &usr_size, &level);
	    src_e = trc_event_lst_add(src, level, usr_size, name, fmt);
	}
	trc_event_lst_bind_extern(target, e,
				    trc_event_lst_extern_id(src, src_e));
	e = trc_event_lst_next(target, e);
    }
}



int
main(int argc, char *argv[])
{
    trc_p           trace_state;
    trc_thread_id_t allXall_threads;
    int             entries;
    struct rlimit   fileres;
    int             n_files;
    char          **file_name;
    char           *out_file = NULL;
    pan_time_fix_p  t_off;
    int             i;
    int             pe;
    trc_t           par_state;
    trc_thread_info_p   thr;
    pan_time_fix_t  t;
    double          d;
    double          dd;
    platf_lst_p     platf_lst = NULL;
    thread_lst_p   *thread_lst;
    boolean         print_rb = FALSE;
    boolean         write_data = TRUE;
    boolean         write_state = TRUE;
    int             n_threads;
    int             n_ev_tp;
    int             events_per_dot = 1000;

    n_files = 0;
    for (i = 1; i < argc; i++) {
	if (argv[i][0] != '-') ++n_files;
    }
    file_name = pan_calloc(n_files, sizeof(char *));
    t_off     = pan_calloc(n_files, sizeof(pan_time_fix_t));
    for (i = 0; i < n_files; i++) {
	t_off[i] = pan_time_fix_infinity;
    }
    n_files = 0;
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    switch (argv[i][1]) {
	    case 'o' :  ++i;
			out_file = argv[i];
			if (strcmp(out_file, "-") == 0) {
			    out_file = NULL;
			}
			break;
	    case 'd' :  ++i;
			if (sscanf(argv[i], "%d", &events_per_dot) != 1) {
			    --i;
			    events_per_dot = INT_MAX;
			}
			break;
	    case 's' :	write_state = FALSE;
			write_data  = FALSE;
			break;
	    case 'r' :	print_rb = TRUE;
			break;
	    case 't' :  ++i;
			if (sscanf(argv[i], "%lf", &d) == 1) {
			    pan_time_fix_d2t(&t_off[n_files], d);
			} else {
			    fprintf(stderr, "Wrong time %s\n", argv[i]);
			}
			break;
	    case 'w' :	write_state = TRUE;
			write_data  = FALSE;
			break;
	    default  :  fprintf(stderr, "No such option %s\n", argv[i]);
	    }
	} else {
	    file_name[n_files] = argv[i];
	    ++n_files;
	}
    }

    if (events_per_dot <= 0) {
	events_per_dot = INT_MAX;
    }

#ifdef AMOEBA
    if (n_files + 3 > FOPEN_MAX) {
	fprintf(stderr, "%s: max files exceeded (%d): split merge!\n",
		argv[0], FOPEN_MAX - 3);
	return 3;
    }
#else
    getrlimit(RLIMIT_NOFILE, &fileres);
    if (n_files + 3 > fileres.rlim_max) {
	fprintf(stderr, "%s: max files exceeded (%ld): split merge!\n",
		argv[0], fileres.rlim_max - 3);
	return 3;
    } else if (n_files + 3 > fileres.rlim_cur) {
	fileres.rlim_cur = n_files + 3;
	setrlimit(RLIMIT_NOFILE, &fileres);
    }
#endif

    trace_state = pan_calloc(n_files, sizeof(trc_t));
    thread_lst  = pan_calloc(n_files, sizeof(thread_lst_p));
    for (i = 0; i < n_files; i++) {
	trc_init_state(&trace_state[i]);
	if (! trc_open_infile(&trace_state[i], file_name[i])) {
	    fprintf(stderr, "%s: file %s not found\n", argv[0], argv[i+1]);
	    --n_files;
	    --i;
	}
    }
    entries = 0;

    trc_init_state(&par_state);
    par_state.event_list = trc_event_lst_init();

    if (events_per_dot != INT_MAX) {
	fprintf(stderr, "(");
    }
    for (i = 0; i < n_files; i++) {
	if (! trc_state_read(&trace_state[i])) {
	    fprintf(stderr, "%s: file %d has no state block", argv[0], i);
	    return 1;
	}
	if (pan_time_fix_cmp(&t_off[i], &pan_time_fix_infinity) != 0) {
	    trace_state[i].t_off   = t_off[i];
	    trace_state[i].t_d_off = pan_time_fix_zero;
	}
	unify_events(par_state.event_list, trace_state[i].event_list);
	if (entries != -1) {
	    if (IS_THREADS_MERGED(trace_state[i].in_buf.format)) {
					/* Find the total number of events.
					 * Done by summing the entries per
					 * file; these are recorded in the
					 * pseudo-threads numbered "-p" for
					 * the processor numbered p. */
		entries += trace_state[i].entries;
	    } else {
					/* If not merged, this file may contain
					 * CLOCK_SHIFT events, so the number of
					 * entries is unreliable. We will count
					 * ourselves. */
		entries = -1;
	    }
	}
	init_platform_buf(&trace_state[i], &platf_lst, &thread_lst[i], i,
			  trace_state[0].t_start);
	if (events_per_dot != INT_MAX) {
	    fprintf(stderr, "o");
	}
    }

    CLOCK_SHIFT     = trc_event_lst_find(par_state.event_list, "clock_shift");
    EMB_CLOCK_SHIFT = trc_event_lst_find(par_state.event_list,
					 "embedded_clock_shift");

    if (events_per_dot != INT_MAX) {
	fprintf(stderr, ")\n");
    }
    par_state.my_pid     = -1;
    par_state.n_pids     = trace_state[0].n_pids;
    par_state.filename   = strdup(trace_state[0].filename);

    par_state.thread     = pan_calloc(par_state.n_pids + 1 + PANDA_1,
				      sizeof(trc_pid_threads_t));
    ++par_state.thread;
    for (pe = -1; pe < par_state.n_pids + PANDA_1; pe++) {
	par_state.thread[pe].threads   = NULL;
	par_state.thread[pe].n_threads = 0;
    }

    n_ev_tp = trc_event_lst_num(par_state.event_list);
    for (i = 0; i < n_files; i++) {
	for (pe = 0; pe < par_state.n_pids + PANDA_1; pe++) {
	    thr = trace_state[i].thread[pe].threads;
	    n_threads = trace_state[i].thread[pe].n_threads;
	    if (n_threads > 0) {
		assert(par_state.thread[pe].n_threads == 0);
		par_state.thread[pe].n_threads = n_threads;
		par_state.n_threads += n_threads;
		par_state.thread[pe].threads = pan_calloc(n_threads,
						     sizeof(trc_thread_info_t));
		while (thr != NULL) {
		    trc_add_thread(&par_state, thr->thread_id, thr->entries,
				   thr->name, trc_event_tp_set_create(n_ev_tp));
		    thr = thr->next_thread;
		}
	    }
	}
    }
    allXall_threads.my_pid    = -1;
    allXall_threads.my_thread = 0;
    par_state.thread[-1].n_threads = 1;
    par_state.thread[-1].threads = pan_calloc(1, sizeof(trc_thread_info_t));
    trc_add_thread(&par_state, allXall_threads, 0, "allXall threads",
		   trc_event_tp_set_create(n_ev_tp));

    par_state.t_off   = pan_time_fix_zero;
    par_state.t_start = trace_state[0].t_start;
    pan_time_fix_sub(&par_state.t_start, &trace_state[0].t_off);
    par_state.t_stop  = trace_state[0].t_stop;
    pan_time_fix_sub(&par_state.t_stop, &trace_state[0].t_off);
    d = pan_time_fix_t2d(&trace_state[0].t_d_off);
    dd = d * d;

    for (i = 1; i < n_files; i++) {
	t = trace_state[i].t_start;
	pan_time_fix_sub(&t, &trace_state[i].t_off);
	if (pan_time_fix_cmp(&par_state.t_start, &t) > 0) {
	    par_state.t_start = t;
	}
	t = trace_state[i].t_stop;
	pan_time_fix_sub(&t, &trace_state[i].t_off);
	if (pan_time_fix_cmp(&par_state.t_stop, &t) < 0) {
	    par_state.t_stop = t;
	}
	d = pan_time_fix_t2d(&trace_state[i].t_off);
	dd += d * d;
    }
    pan_time_fix_d2t(&par_state.t_d_off, sqrt(dd));

    par_state.rebind = trc_init_rebind();
    trc_mk_obj_ev(par_state.event_list, par_state.rebind, NULL, 0, FALSE);

    for (i = 0; i < n_files; i++) {
	trc_bind_merge(trace_state[i].rebind, par_state.rebind);
    }

    trc_open_outfile(&par_state, out_file, OUT_BUF_SIZE, allXall_threads);

    par_state.entries = entries;

    par_state.out_buf.format |= PLATFORMS_MERGED | OBJECTS_TICKED |
			        EVENTS_TICKED;

    par_state.entries = merge_platforms(&par_state, trace_state, thread_lst,
					n_files, &platf_lst, write_data,
					events_per_dot);
    if (write_state) {
	if (events_per_dot != INT_MAX) {
	    fprintf(stderr, "(");
	}
	trc_state_write(&par_state);
	if (events_per_dot != INT_MAX) {
	    fprintf(stderr, ")(");
	}
    }

    for (i = 0; i < n_files; i++) {
	trc_close_files(&trace_state[i]);
	trc_state_clear(&trace_state[i]);
	if (events_per_dot != INT_MAX) {
	    fprintf(stderr, "x");
	}
    }
    if (events_per_dot != INT_MAX) {
	fprintf(stderr, ")\n");
    }

    trc_close_files(&par_state);

    if (print_rb) {
	trc_rebind_print(stderr, par_state.rebind);
    }

    trc_state_clear(&par_state);

    pan_free(trace_state);
    pan_free(thread_lst);

    pan_free(file_name);
    pan_free(t_off);

    return 0;
}
