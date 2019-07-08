/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>

#include "pan_sys.h"

#include "pan_util.h"

#include "trc_types.h"
#include "trc_event_tp.h"
#include "trc_lib.h"
#include "trc_io.h"
#include "trc_bind_op.h"



#define TRC_EVENTS 256

const char  trc_marker[] = "========================================\n";

static char dummy_str[] = "No such buf";


typedef enum BLOCK_TP {
    TRC_FMT_BLOCK	= 0x0,
    TRC_DATA_BLOCK	= 0x1
}           block_tp_t, *block_tp_p;


					/* The format in which events are
					 * saved initially. Thread id is
					 * derived from the global event data
					 * block to which the event belongs. */
typedef struct TRC_IMPLICIT_T {
    trc_event_t	    type;
    pan_time_diff_t dt;
} trc_implicit_t, *trc_implicit_p;


					/* The format in which events are
					 * saved after trc_merge. */
typedef struct TRC_COMPACT_T {
    trc_event_t	type;
    union {
	pan_time_fix_t	t_clock_shift;
	struct {
	    long int	    dt;
	    trc_thread_id_t thread_id;
	} ev_datum;
    }		datum;
} trc_compact_t, *trc_compact_p;


typedef struct TRC_OLD_THREAD_ID_T {
    int my_pid;
    int my_thread;
} trc_old_thread_id_t, *trc_old_thread_id_p;



#define EXPLICIT_DESCR_LEN	(sizeof(trc_event_t) + \
				 sizeof(trc_thread_id_t) + \
				 sizeof(pan_time_fix_t))

#define IMPLICIT_DESCR_LEN	(sizeof(trc_event_t) + sizeof(long int))


#define is_digit(c) ((c) >= '0' && (c) <= '9')

int
trc_check_int(char *fmt, int *count)
{
    boolean     is_neg = FALSE;
    int         val;
    char       *fmt_start = fmt;

    if (*fmt == '-') {
	is_neg = TRUE;
	++fmt;
    }
    val = 0;
    while (is_digit(*fmt)) {
	val = 10 * val + (*fmt - '0');
	++fmt;
    }
    if (is_neg)
	val = -val;
    *count = val;
    return fmt - fmt_start;
}


typedef union INT_CHARS {
    int i;
    char c[4];
} int_chars_t;


#ifdef NO_MACROS
static void
int_swap_b(int *i)
{
    int_chars_t cp;
    int i;

    cp.c[0] = ((int_chars_p)i)->c[3];
    cp.c[1] = ((int_chars_p)i)->c[2];
    cp.c[2] = ((int_chars_p)i)->c[1];
    cp.c[3] = ((int_chars_p)i)->c[0];
    *i = cp.i;
}


static void
uint_swap_b(unsigned int *u)
{
    int_swap_b((int *)u);
}
#endif		/* NO_MACROS */


#define swap_b(p, tp) \
	do { \
	    tp _swap_n; \
	    int _swap_i; \
	    int _swap_j; \
	    _swap_j = sizeof(tp) - 1; \
	    for (_swap_i = 0; _swap_i < sizeof(tp); _swap_i++, _swap_j--) { \
		((char *)&_swap_n)[_swap_i] = ((char*)p)[_swap_j]; \
	    } \
	    *p = _swap_n; \
	} while (0);


#define int_swap_b(i)		swap_b(i, int)
#define uint_swap_b(u)		swap_b(u, unsigned int)
#define sint_swap_b(s)		swap_b(s, short int)
#define lint_swap_b(l)		swap_b(l, long int)
#define double_swap_b(d)	swap_b(d, double)
#define ptr_swap_b(p)		swap_b(p, void *)



static void
old_thread_id_swap_b(trc_old_thread_id_p id)
{
    int_swap_b(&id->my_pid);
    int_swap_b(&id->my_thread);
}


static void
thread_id_swap_b(trc_thread_id_p id)
{
    sint_swap_b(&id->my_pid);
    sint_swap_b(&id->my_thread);
}


static void
pan_time_fix_swap_b(pan_time_fix_p t)
{
    int_swap_b(&t->t_sec);
    int_swap_b(&t->t_nsec);
}


static char *
get_next_percent(char *fmt)
{
    do {
	while (*fmt != '\0' && *fmt != '%')
	    ++fmt;

	if (fmt[0] == '\0')
	    return NULL;

	if (fmt[1] == '%') {
	    fmt += 2;
	} else {
	    return fmt;
	}
    } while (TRUE);
}



static void
swap_one_tok(char fmt_c, char *buf, size_t *n_buf)
{
    char *buf_p;
 
    buf_p = buf + *n_buf;
 
    switch (fmt_c) {
    case 'f':
	double_swap_b((double*)buf_p);
	buf_p += sizeof(double);
	break;
    case 'd':
	int_swap_b((int*)buf_p);
	buf_p += sizeof(int);
	break;
    case 'p':
	ptr_swap_b((void**)buf_p);
	buf_p += sizeof(void*);
	break;
    case 'h':
	sint_swap_b((short int*)buf_p);
	buf_p += sizeof(short int);
	break;
    case 'l':
	lint_swap_b((long int*)buf_p);
	buf_p += sizeof(long int);
	break;
    case 'b':
	++buf_p;
	break;
    case 'c':
	++buf_p;
	break;
    default:
	fprintf(stderr, "%%%c conversion not supported\n", fmt_c);
    }
 
    *n_buf = buf_p - buf;
}



static void
trc_usr_swap_b(char *usr, char *fmt)
{
    char  fmt_c;
    char *fmt_p;
    int   n;
    int   count;
    size_t n_usr;

    fmt_p = fmt;
    n_usr = 0;

    do {

	fmt_p = get_next_percent(fmt_p);
	if (fmt_p == NULL) {
	    return;
	}

	++fmt_p;			/* skip the '%' */

	n = trc_check_int(fmt_p, &count);	/* fmt prefixed by a count? */
	if (n != 0) {
	    fmt_p += n;			/* skip the digits */
	} else {
	    count = 1;
	}

	fmt_c = fmt_p[0];
	++fmt_p;			/* skip the format char */

	if (fmt_c == 's') {
	    n_usr += count * sizeof(char);
	} else {
	    for (n = 0; n < count; n++) {
		swap_one_tok(fmt_c, usr, &n_usr);
	    }
	}

    } while (TRUE);
}


void
trc_event_lst_write(FILE *s, trc_event_lst_p lst)
{
    trc_event_t num;
    int         len;
    trc_event_t e;
    size_t      usr_size;
    int         level;
    trc_event_t e_extern;
    char       *name;
    char       *fmt;
    int         n;

    num = trc_event_lst_num(lst);
    if ((n = fwrite((char *)&num, sizeof(trc_event_t), 1, s)) != 1)
	pan_panic("Error writing #event bindings; %d\n", n);

    e = trc_event_lst_first(lst);
    while (e != TRC_NO_SUCH_EVENT) {
	trc_event_lst_query(lst, e, &usr_size, &level);
	trc_event_lst_query_extern(lst, e, &e_extern, &name, &fmt);
	if (fwrite((char *)&e, sizeof(trc_event_t), 1, s) != 1)
	    pan_panic("Error writing event %d #\n", num);
	if (fwrite((char *)&e_extern, sizeof(trc_event_t), 1, s) != 1)
	    pan_panic("Error writing event %d #\n", num);
	if (fwrite((char *)&level, sizeof(int), 1, s) != 1)
	    pan_panic("Error writing event %d level\n", num);
	if (fwrite((char *)&usr_size, sizeof(size_t), 1, s) != 1)
	    pan_panic("Error writing event %d usr_size\n", num);
	len = strlen(name) + 1;
	if (fwrite((char *)&len, sizeof(int), 1, s) != 1)
	    pan_panic("Error writing event %d name length\n", num);
	if (fwrite(name, 1, len, s) != len)
	    pan_panic("Error writing event %d name\n", num);
	len = strlen(fmt) + 1;
	if (fwrite((char *)&len, sizeof(int), 1, s) != 1)
	    pan_panic("Error writing event %d fmt length\n", num);
	if (fwrite(fmt, 1, len, s) != len)
	    pan_panic("Error writing event %d fmt\n", num);
	e = trc_event_lst_next(lst, e);
	--num;
    }
    assert(num == 0);
}


static void
trc_event_lst_read(FILE *s, trc_event_lst_p lst, boolean little_endian)
{
    int         i;
    int         level;
    size_t      usr_size;
    char       *name;
    char       *fmt;
    trc_event_t num;
    trc_event_t e_extern;
    trc_event_t n_events;
    int         len;

    if (fread((char *)&n_events, sizeof(trc_event_t), 1, s) != 1)
	pan_panic("Error reading #event bindings\n");
    if (little_endian) int_swap_b(&n_events);

    for (i = 0; i < n_events; i++) {
	if (fread((char *)&num,      sizeof(trc_event_t), 1, s) != 1)
	    pan_panic("Error reading event %d type\n", i);
	if (fread((char *)&e_extern, sizeof(trc_event_t), 1, s) != 1)
	    pan_panic("Error reading event %d extern type\n", i);
	if (fread((char *)&level,    sizeof(int),         1, s) != 1)
	    pan_panic("Error reading event %d level\n", i);
	if (fread((char *)&usr_size, sizeof(size_t),      1, s) != 1)
	    pan_panic("Error reading event %d usr_size\n", i);
	if (fread((char *)&len,      sizeof(int),         1, s) != 1)
	    pan_panic("Error reading event %d name length\n", i);

	if (little_endian) {
	    int_swap_b(&num);
	    int_swap_b(&e_extern);
	    int_swap_b(&level);
	    uint_swap_b(&usr_size);
	    int_swap_b(&len);
	}

	name = pan_malloc(len);
	if (fread(name, 1, len, s) != len)
	    pan_panic("Error reading event %d name\n", i);
	if (fread((char *)&len,      sizeof(int),         1, s) != 1)
	    pan_panic("Error reading event %d fmt length\n", i);
	if (little_endian) int_swap_b(&len);

	fmt = pan_malloc(len);
	if (fread(fmt, 1, len, s) != len)
	    pan_panic("Error reading event %d fmt\n", i);
	trc_event_lst_bind(lst, num, e_extern, level, usr_size, name, fmt);
	pan_free(name);
	pan_free(fmt);
    }
}


static void
trc_event_lst_skip(trc_p trc)
{
    FILE       *s;
    int         i;
    trc_event_t n_events;
    int         len;
    boolean     little_endian = IS_LITTLE_ENDIAN(trc->in_buf.format);

    s = trc->in_buf.stream;

    if (fread((char *)&n_events, sizeof(trc_event_t), 1, s) != 1)
	pan_panic("Error reading #event bindings\n");
    if (little_endian) int_swap_b(&n_events);

    for (i = 0; i < n_events; i++) {
	if (fseek(s, sizeof(trc_event_t), 1) == -1)
	    pan_panic("seek of event_lst/num failed\n");
	if (fseek(s, sizeof(trc_event_t), 1) == -1)
	    pan_panic("seek of event_lst/extern_id failed\n");
	if (fseek(s, sizeof(int), 1) == -1)
	    pan_panic("seek of event_lst/level failed\n");
	if (fseek(s, sizeof(size_t), 1) == -1)
	    pan_panic("seek of event_lst/size failed\n");
	if (fread((char *)&len, sizeof(int), 1, s) != 1)
	    pan_panic("Error reading event %d name length\n", i);
	if (little_endian) int_swap_b(&len);

	if (fseek(s, len, 1) == -1)
	    pan_panic("seek of event_lst/name failed\n");
	if (fread((char *)&len, sizeof(int), 1, s) != 1)
	    pan_panic("Error reading event %d fmt length\n", i);
	if (little_endian) int_swap_b(&len);

	if (fseek(s, len, 1) == -1)
	    pan_panic("seek of event_lst/fmt failed\n");
    }
}



static void
trc_fmt_skip(trc_p trc)
{
    FILE       *s;
    int         i;
    int         n;
    int         len;
    int         n_pids;
    int         pe;
    int         ev_tp_bytes;
    boolean     little_endian = IS_LITTLE_ENDIAN(trc->in_buf.format);

    s = trc->in_buf.stream;

    if (fseek(s, sizeof(int), 1) == -1)
	pan_panic("error seeking past my_pid\n");
    if (fread((char *)&n_pids, sizeof(int), 1, s) != 1)
	pan_panic("error reading nr_pids\n");
    if (little_endian) int_swap_b(&n_pids);

    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past offset.t_sec\n");
    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past offset.t_nsec\n");
    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past d_offset.t_sec\n");
    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past d_offset.t_nsec\n");
    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past start.t_sec\n");
    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past start.t_nsec\n");
    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past now.t_sec\n");
    if (fseek(s, sizeof(long int), 1) == -1)
	pan_panic("error seeking past now.t_nsec\n");
    if (fseek(s, sizeof(trc_format_t), 1) == -1)
	pan_panic("error seeking past trc_format\n");
    if (fseek(s, sizeof(int), 1) == -1)
	pan_panic("error seeking past entries\n");
    if (trc->in_buf.format & EVENTS_TICKED) {
	if (fseek(s, sizeof(int), 1) == -1)
	    pan_panic("error seeking past n_event_types\n");
    }
    if (fread((char *)&len, sizeof(int), 1, s) != 1)
	pan_panic("error reading fmt/indent string length\n");
    if (little_endian) int_swap_b(&len);

    if (fseek(s, len, 1) == -1)
	pan_panic("error seeking past fmt/ident string\n");
    if (fseek(s, sizeof(int), 1) == -1)
	pan_panic("error seeking past # threads\n");

    if (trc->in_buf.format & EVENTS_TICKED) {
	ev_tp_bytes = trc_event_tp_set_length(trc->n_event_types);
    }

    for (pe = -1; pe < n_pids + PANDA_1; pe++) {
	if (fread((char *)&n, sizeof(int), 1, s) != 1)
	    pan_panic("error reading # threads/pe\n");
	if (little_endian) int_swap_b(&n);

	for (i = 0; i < n; i++) {
	    if (fseek(s, sizeof(int), 1) == -1)
		pan_panic("error seeking past fmt/thread_id.thread\n");
	    if (fseek(s, sizeof(int), 1) == -1)
		pan_panic("error seeking past fmt/thread_id.pid\n");
	    if (fseek(s, sizeof(int), 1) == -1)
		pan_panic("error seeking past fmt/entries\n");
	    if (fread((char *)&len, sizeof(int), 1, s) != 1)
		pan_panic("error reading thread name length %d\n", i);
	    if (little_endian) int_swap_b(&len);

	    if (fseek(s, len, 1) == -1)
		pan_panic("error seeking past fmt/thread name\n");

	    if (trc->in_buf.format & EVENTS_TICKED) {
		if (fseek(s, ev_tp_bytes, 1) == -1) {
		    pan_panic("error seeking past event set\n");
		}
	    }
	}
    }
}



static void
trc_state_skip(trc_p trc)
{
    trc_fmt_skip(trc);
    trc_event_lst_skip(trc);
    if (trc->in_buf.format & OBJECTS_TICKED) {
	trc_rebind_skip(trc->in_buf.stream);
    }
}


static void
trc_fmt_read(trc_p trc)
{
    trc_thread_id_t thread_id;
    char       *name;
    int         i;
    int         len;
    int         entries;
    int         pe;
    int         p;
    int         n_threads;
    int         ev_tp_bytes;
    trc_event_tp_set_p set;
    FILE       *s;
    boolean     little_endian = IS_LITTLE_ENDIAN(trc->in_buf.format);

    s = trc->in_buf.stream;
    if (fread((char *)&trc->my_pid,         sizeof(int), 1, s) != 1)
	pan_panic("error reading my_pid\n");
    if (fread((char *)&trc->n_pids,         sizeof(int), 1, s) != 1)
	pan_panic("error reading n_pids\n");
    if (fread((char *)&trc->t_off.t_sec,    sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace offset t.t_sec\n");
    if (fread((char *)&trc->t_off.t_nsec,   sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace offset t.t_nsec\n");
    if (fread((char *)&trc->t_d_off.t_sec,  sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace offset t.t_sec\n");
    if (fread((char *)&trc->t_d_off.t_nsec, sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace offset t.t_nsec\n");
    if (fread((char *)&trc->t_start.t_sec,  sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace start t.t_sec\n");
    if (fread((char *)&trc->t_start.t_nsec, sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace start t.t_nsec\n");
    if (fread((char *)&trc->t_stop.t_sec,   sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace stop t.t_sec\n");
    if (fread((char *)&trc->t_stop.t_nsec,  sizeof(long int), 1, s) != 1)
	pan_panic("error reading trace stop t.t_nsec\n");
    if (fread((char *)&trc->in_buf.format,  sizeof(trc_format_t), 1, s) != 1)
	pan_panic("error reading trc_format\n");
    if (fread((char *)&trc->entries,        sizeof(int), 1, s) != 1)
	pan_panic("error reading entries\n");
    if (trc->in_buf.format & EVENTS_TICKED) {
	if (fread((char *)&trc->n_event_types, sizeof(int), 1, s) != 1)
	    pan_panic("error reading entries\n");
    }
    if (fread((char *)&len,                 sizeof(int), 1, s) != 1)
	pan_panic("error reading fmt/ident string length\n");

    if (little_endian) {
	int_swap_b(&trc->my_pid);
	int_swap_b(&trc->n_pids);
	pan_time_fix_swap_b(&trc->t_off);
	pan_time_fix_swap_b(&trc->t_d_off);
	pan_time_fix_swap_b(&trc->t_start);
	pan_time_fix_swap_b(&trc->t_stop);
	int_swap_b(&trc->in_buf.format);
	int_swap_b(&trc->entries);
	int_swap_b(&trc->n_event_types);
	int_swap_b(&len);
    }

    trc->filename = pan_malloc(len);
    if (fread(trc->filename, 1, len, s) != len)
	pan_panic("error reading fmt/ident string\n");
    if (fread((char *)&n_threads, sizeof(int), 1, s) != 1)
	pan_panic("error reading # threads\n");
    if (little_endian) int_swap_b(&n_threads);

    if (trc->in_buf.format & EVENTS_TICKED) {
	ev_tp_bytes = trc_event_tp_set_length(trc->n_event_types);
    }

    trc->n_threads = 0;
    trc->thread = pan_calloc((trc->n_pids + 2), sizeof(trc_pid_threads_t));
    trc->thread += 1;

    for (pe = -1; pe < trc->n_pids + PANDA_1; pe++) {
	if (fread((char *)&n_threads, sizeof(int), 1, s) != 1)
	    pan_panic("error reading # threads/pe\n");
	if (little_endian) int_swap_b(&n_threads);

	if (n_threads == 0) {
	    trc->thread[pe].threads = NULL;
	} else {
	    trc->thread[pe].threads = pan_calloc(n_threads,
						 sizeof(trc_thread_info_t));
	}
	if (pe != -1) {
	    trc->n_threads += n_threads;
	}
	trc->thread[pe].n_threads = n_threads;
	for (i = 0; i < n_threads; i++) {
	    if (fread((char *)&p,       sizeof(int), 1, s) != 1)
		pan_panic("error reading thread id %d\n", i);
	    if (little_endian) int_swap_b(&p);
	    thread_id.my_thread = (short int)p;
	    if (fread((char *)&p,       sizeof(int), 1, s) != 1)
		pan_panic("error reading thread id %d\n", i);
	    if (little_endian) int_swap_b(&p);
	    thread_id.my_pid = (short int)p;
	    assert(thread_id.my_pid == pe);

	    if (fread((char *)&entries, sizeof(int), 1, s) != 1)
		pan_panic("error reading thread entries %d\n", i);
	    if (fread((char *)&len,     sizeof(int), 1, s) != 1)
		pan_panic("error reading thread name length %d\n", i);

	    if (little_endian) {
		int_swap_b(&entries);
		int_swap_b(&len);
	    }

	    name = pan_malloc(len);
	    if (fread(name, 1, len, s) != len)
		pan_panic("error reading thread name %d\n", i);

	    if (trc->in_buf.format & EVENTS_TICKED) {
		set = trc_event_tp_set_create(trc->n_event_types);
		if (fread((char *)set, ev_tp_bytes, 1, s) != 1) {
		    pan_panic("error reading event set\n");
		}
	    }

	    trc_add_thread(trc, thread_id, entries, name, set);
	    pan_free(name);
	}
    }
}


static void
trc_fmt_write(trc_p trc)
{
    trc_thread_info_p p;
    int               len;
    int               pe;
    int               n_threads;
    int               n_ev_tp;
    int               ev_tp_bytes;
    FILE             *s;

    s = trc->out_buf.stream;
    fwrite((char *)&trc->my_pid,         sizeof(int),          1, s);
    fwrite((char *)&trc->n_pids,         sizeof(int),          1, s);
    fwrite((char *)&trc->t_off.t_sec,    sizeof(long int),     1, s);
    fwrite((char *)&trc->t_off.t_nsec,   sizeof(long int),     1, s);
    fwrite((char *)&trc->t_d_off.t_sec,  sizeof(long int),     1, s);
    fwrite((char *)&trc->t_d_off.t_nsec, sizeof(long int),     1, s);
    fwrite((char *)&trc->t_start.t_sec,  sizeof(long int),     1, s);
    fwrite((char *)&trc->t_start.t_nsec, sizeof(long int),     1, s);
    fwrite((char *)&trc->t_stop.t_sec,   sizeof(long int),     1, s);
    fwrite((char *)&trc->t_stop.t_nsec,  sizeof(long int),     1, s);
    fwrite((char *)&trc->out_buf.format, sizeof(trc_format_t), 1, s);
    fwrite((char *)&trc->entries,        sizeof(int),          1, s);
    if (trc->out_buf.format & EVENTS_TICKED) {
	n_ev_tp = trc_event_lst_num(trc->event_list);
	fwrite((char *)&n_ev_tp,         sizeof(int),          1, s);
    }
    len = strlen(trc->filename) + 1;
    fwrite((char *)&len,                 sizeof(int),          1, s);
    fwrite(trc->filename,                1,                  len, s);
    fwrite((char *)&trc->n_threads,      sizeof(int),          1, s);

    ev_tp_bytes = trc_event_tp_set_length(n_ev_tp);
    for (pe = -1; pe < trc->n_pids + PANDA_1; pe++) {
	fwrite((char *)&trc->thread[pe].n_threads, sizeof(int), 1, s);
	p = trc->thread[pe].threads;
	n_threads = 0;
	while (p != NULL) {
	    assert(n_threads == p->thread_id.my_thread);
	    fwrite((char *)&n_threads, sizeof(int), 1, s);
	    assert(p->thread_id.my_pid == pe);
	    fwrite((char *)&pe, sizeof(int), 1, s);
	    fwrite((char *)&p->entries, sizeof(int), 1, s);
	    len = strlen(p->name) + 1;
	    fwrite((char *)&len, sizeof(int), 1, s);
	    fwrite(p->name, 1, len, s);
	    if (trc->out_buf.format & EVENTS_TICKED) {
		fwrite((char *)p->event_tp_set, ev_tp_bytes, 1, s);
	    }

	    ++n_threads;
	    p = p->next_thread;
	}
	assert(n_threads == trc->thread[pe].n_threads);
    }
}


static void
trc_clear_threads(trc_p trc)
{
    int               pe;
    trc_thread_info_p thr;
    trc_thread_info_p hold;

    for (pe = -1; pe < trc->n_pids + PANDA_1; pe++) {
	thr = trc->thread[pe].threads;
	while (thr != NULL) {
	    hold = thr;
	    thr = thr->next_thread;
	    pan_free(hold->name);
	}
    }
}


static void
trc_fmt_clear(trc_p trc)
{
    pan_free(trc->filename);
    trc_clear_threads(trc);
    if (trc->thread != NULL) {
	--trc->thread;
	pan_free(trc->thread);
    }
}



static void
trc_state_block_read(trc_p trc)
{
    trc_fmt_read(trc);

    trc->event_list = trc_event_lst_init();
    trc_event_lst_read(trc->in_buf.stream, trc->event_list,
		       IS_LITTLE_ENDIAN(trc->in_buf.format));

    if (trc->in_buf.format & OBJECTS_TICKED) {
	trc->rebind = trc_rebind_read(trc->in_buf.stream);
	trc_mk_obj_ev(trc->event_list, trc->rebind, NULL, 0, FALSE);
    } else {
	trc->rebind = NULL;
    }

    /* START_TRACE = trc_event_lst_find(trc->event_list, "START_TRACE"); */
    /* END_TRACE   = trc_event_lst_find(trc->event_list, "END_TRACE"); */
    if (IS_COMPACT_FORMAT(trc->in_buf.format) &&
	IS_EXPLICIT_SRC(trc->in_buf.format)) {
	EMB_CLOCK_SHIFT = trc_event_lst_find(trc->event_list,
						"embedded_clock_shift");
    } else {
	EMB_CLOCK_SHIFT = trc_event_lst_add(trc->event_list, USHRT_MAX, 0,
					    "embedded_clock_shift", "");
    }
    CLOCK_SHIFT = trc_event_lst_find(trc->event_list, "clock_shift");
    FLUSH_BLOCK = trc_event_lst_find(trc->event_list, "flush_block");
}



void
trc_state_write(trc_p trc)
{
    char        pre_hdr[sizeof(block_tp_t)];

    pre_hdr[0] = IS_LITTLE_ENDIAN(trc->out_buf.format);
    pre_hdr[1] = IS_COMPACT_FORMAT(trc->out_buf.format);
    pre_hdr[sizeof(block_tp_t) - 1] = (char)TRC_FMT_BLOCK;

    fwrite(pre_hdr, 1, sizeof(block_tp_t), trc->out_buf.stream);

    trc_fmt_write(trc);

    trc_event_lst_write(trc->out_buf.stream, trc->event_list);

    if (trc->out_buf.format & OBJECTS_TICKED) {
	trc_rebind_write(trc->out_buf.stream, trc->rebind);
    }
}


static void
trc_block_hdr_read(trc_p trc, trc_block_hdr_p hdr)
{
    FILE *s;

    s = trc->in_buf.stream;

    fread((char *)&hdr->size,      sizeof(size_t), 1, s);

    if (IS_COMPACT_FORMAT(trc->in_buf.format)) {
	fread((char *)&hdr->thread_id, sizeof(trc_thread_id_t), 1, s);
	if (IS_LITTLE_ENDIAN(trc->in_buf.format)) {
	    thread_id_swap_b(&hdr->thread_id);
	    uint_swap_b(&hdr->size);
	}
    } else {
	trc_old_thread_id_t id;
	fread((char *)&id, sizeof(trc_old_thread_id_t), 1, s);
	if (IS_LITTLE_ENDIAN(trc->in_buf.format)) {
	    old_thread_id_swap_b(&id);
	    uint_swap_b(&hdr->size);
	}
	hdr->thread_id.my_pid = id.my_pid;
	hdr->thread_id.my_thread = id.my_thread;
    }
}



static void
trc_block_skip(trc_p trc)
{
    trc_block_hdr_t hdr;

					/* read the header */
    trc_block_hdr_read(trc, &hdr);
					/* skip the data */
    if (fseek(trc->in_buf.stream, hdr.size, 1) == -1)
	pan_panic("seek of data buf failed\n");
}


boolean
trc_state_read(trc_p trc)
{
    block_tp_t  block_tp;
    boolean     prelim_read = FALSE;
    char        pre_hdr[sizeof(block_tp_t)];

    while (TRUE) {
	fread(pre_hdr, 1, sizeof(block_tp_t), trc->in_buf.stream);
	if (feof(trc->in_buf.stream)) {
	    return prelim_read;
	}

	if (pre_hdr[0]) {
	    trc->in_buf.format |= LITTLE_ENDIAN;
	}
	if (pre_hdr[1]) {
	    trc->in_buf.format |= COMPACT_FORMAT;
	}
	block_tp = (block_tp_t)pre_hdr[sizeof(block_tp_t) - 1];

	switch (block_tp & 0x01) {
	case TRC_FMT_BLOCK:
	    trc_state_block_read(trc);
	    trc->out_buf.format = trc->in_buf.format;
	    trc->out_buf.format |= COMPACT_FORMAT;
	    if (IS_LITTLE_ENDIAN(trc->in_buf.format)) {
		trc->out_buf.format &= ~LITTLE_ENDIAN;
	    }
	    trc->out_buf.format &= ~STATE_FIRST;
	    prelim_read = trc->entries == -1;
	    if (!prelim_read) {
		if (!STATE_COMES_FIRST(trc->in_buf.format)) {
		    fseek(trc->in_buf.stream, 0, 0);
		}
		return TRUE;
	    }
	    break;
	case TRC_DATA_BLOCK:
	    trc_block_skip(trc);
	    break;
	default:
	    pan_panic("Illegal trace file block\n");
	    exit(10);
	}
    }
}



void
trc_state_clear(trc_p trc)
{
    trc_fmt_clear(trc);
    trc_event_lst_clear(trc->event_list);
    trc->event_list = NULL;
    EMB_CLOCK_SHIFT = -1;
    CLOCK_SHIFT = -1;
    FLUSH_BLOCK = -1;
}


/* Non-compacted implicit thread_id in file */
static size_t
trc_event_descr_get_i_old(char *buf, boolean little_endian, trc_event_descr_p e)
{
    char       *p;

    p = buf;

    memcpy(&e->type, p, sizeof(trc_event_t));
    if (little_endian) int_swap_b(&e->type);
    p += sizeof(trc_event_t);
    e->t.t_sec = 0;
    memcpy(&e->t.t_nsec, p, sizeof(long int));
    if (little_endian) int_swap_b(&e->t.t_nsec);
    p += sizeof(long int);


    return p - buf;
}



/* Non-compacted explicit thread_id in file */
static size_t
trc_event_descr_get_e_old(char *buf, boolean little_endian, trc_event_descr_p e)
{
    char       *p;
    trc_old_thread_id_t thread_id;

    p = buf;

    memcpy(&e->type, p, sizeof(trc_event_t));
    p += sizeof(trc_event_t);
    memcpy(&thread_id, p, sizeof(trc_old_thread_id_t));
    if (little_endian) {
	int_swap_b(&e->type);
	old_thread_id_swap_b(&thread_id);
    }
    p += sizeof(trc_old_thread_id_t);
    e->thread_id.my_pid = thread_id.my_pid;
    e->thread_id.my_thread = thread_id.my_thread;
    memcpy(&e->t, p, sizeof(pan_time_fix_t));
    if (little_endian) pan_time_fix_swap_b(&e->t);
    p += sizeof(pan_time_fix_t);

    return p - buf;
}



/* Compacted implicit thread_id in file */
static size_t
trc_event_descr_get_i(char *buf, pan_time_fix_p t_offset,
		      boolean little_endian, trc_event_descr_p e)
{
    trc_implicit_p ei = (trc_implicit_p)buf;

    e->type = ei->type;
    if (little_endian) int_swap_b(&e->type);

    if (e->type != CLOCK_SHIFT) {
	e->t.t_sec   = 0;
	e->t.t_nsec  = ei->dt;
	if (little_endian) int_swap_b(&e->t.t_nsec);
    }

    return sizeof(trc_event_t) + sizeof(pan_time_diff_t);
}


/* Compacted explicit thread_id in file */
static size_t
trc_event_descr_get_e(char *buf, pan_time_fix_p t_offset,
		      boolean little_endian, trc_event_descr_p e)
{
    trc_compact_p ec = (trc_compact_p)buf;

    e->type = ec->type;
    if (little_endian) int_swap_b(&e->type);

    if (e->type == EMB_CLOCK_SHIFT) {
	e->t = ec->datum.t_clock_shift;
	if (little_endian) {
	    pan_time_fix_swap_b(&e->t);
	}
    } else {
	e->t.t_sec   = 0;
	e->t.t_nsec  = ec->datum.ev_datum.dt;
	e->thread_id = ec->datum.ev_datum.thread_id;
	if (little_endian) {
	    int_swap_b(&e->t.t_nsec);
	    thread_id_swap_b(&e->thread_id);
	}
	pan_time_fix_add(&e->t, t_offset);
    }

    *t_offset = e->t;

    return sizeof(trc_compact_t);
}



static int
trc_event_descr_get(char *buf, trc_p trc, trc_event_descr_p e)
{
    trc_format_t   format = trc->in_buf.format;
    boolean        little_endian = IS_LITTLE_ENDIAN(format);
    pan_time_fix_p t;

    if (IS_COMPACT_FORMAT(format)) {
	t = &trc->in_buf.t_current;
	if (IS_EXPLICIT_SRC(format)) {
	    return trc_event_descr_get_e(buf, t, little_endian, e);
	} else {
	    return trc_event_descr_get_i(buf, t, little_endian, e);
	}
    } else {
	if (IS_EXPLICIT_SRC(format)) {
	    return trc_event_descr_get_e_old(buf, little_endian, e);
	} else {
	    return trc_event_descr_get_i_old(buf, little_endian, e);
	}
    }
}



size_t
trc_event_get(char *buf, trc_p trc, trc_event_descr_p e)
{
    size_t       usr_size;
    char        *buf_start = buf;
    trc_format_t format = trc->in_buf.format;
    boolean      is_little_endian = IS_LITTLE_ENDIAN(format);

    do {

	buf += trc_event_descr_get(buf, trc, e);

	usr_size = trc_event_lst_usr_size(trc->event_list, e->type);

	if (usr_size == 0) {
	    if (e->usr_size > 0) {
		pan_free(e->usr);
	    }
	} else {
	    if (usr_size > e->usr_size) {
		if (e->usr_size != 0) {
		    pan_free(e->usr);
		}
		e->usr = pan_malloc(usr_size);
	    }
	    memcpy(e->usr, buf, usr_size);

	    if (is_little_endian) {
		trc_usr_swap_b(e->usr, trc_event_lst_fmt(trc->event_list,
							 e->type));
	    }

	    buf += usr_size;
	}
	e->usr_size = usr_size;
    
    } while (e->type == EMB_CLOCK_SHIFT && IS_EXPLICIT_SRC(format) &&
	     IS_COMPACT_FORMAT(format));

    return buf - buf_start;
}


static size_t
trc_event_read(trc_p trc, trc_event_descr_p e)
{
    size_t      size;
    size_t      usr_size;
    FILE       *s;
    char        buf[EXPLICIT_DESCR_LEN];

    s = trc->in_buf.stream;

    if (IS_COMPACT_FORMAT(trc->in_buf.format)) {
	if (IS_EXPLICIT_SRC(trc->in_buf.format)) {
	    fread(buf, 1, sizeof(trc_compact_t), s);
	} else {
	    fread(buf, 1, sizeof(trc_implicit_t), s);
	}
    } else {
	if (IS_EXPLICIT_SRC(trc->in_buf.format)) {
	    fread(buf, 1, EXPLICIT_DESCR_LEN, s);
	} else {
	    fread(buf, 1, IMPLICIT_DESCR_LEN, s);
	}
    }

    size = trc_event_descr_get(buf, trc, e);

    usr_size = trc_event_lst_usr_size(trc->event_list, e->type);
    if (usr_size == 0) {
	if (e->usr_size > 0) {
	    pan_free(e->usr);
	}
    } else {
	if (usr_size > e->usr_size) {
	    if (e->usr_size != 0) {
		pan_free(e->usr);
	    }
	    e->usr = pan_malloc(usr_size);
	}
	fread(e->usr, usr_size, 1, s);

	if (IS_LITTLE_ENDIAN(trc->in_buf.format)) {
	    trc_usr_swap_b(e->usr, trc_event_lst_fmt(trc->event_list, e->type));
	}

	size += usr_size;
    }

    e->usr_size = usr_size;

    if (e->type == CLOCK_SHIFT) {
	memcpy(&e->t, e->usr, sizeof(pan_time_fix_t));
	trc->in_buf.t_current = e->t;
    }

    pan_time_fix_sub(&e->t, &trc->t_off);

    return size;
}


static void
trc_event_write(trc_p trc, trc_event_descr_p e)
{
    trc_compact_t ec;
    FILE *s = trc->out_buf.stream;
    pan_time_fix_t dt;

    dt = e->t;
    pan_time_fix_sub(&dt, &trc->out_buf.t_current);
    trc->out_buf.t_current = e->t;

    if (dt.t_sec != 0) {
	ec.type = EMB_CLOCK_SHIFT;
	ec.datum.t_clock_shift = e->t;
	fwrite((char *)&ec, sizeof(trc_compact_t), 1, s);
	dt.t_nsec = 0;
    }

    ec.type = e->type;
    ec.datum.ev_datum.dt        = dt.t_nsec;
    ec.datum.ev_datum.thread_id = e->thread_id;
    fwrite((char *)&ec, sizeof(trc_compact_t), 1, s);
    fwrite(e->usr,      e->usr_size,           1, s);
}


void
trc_add_thread(trc_p trc, trc_thread_id_t thread_id, int entries, char *name,
	       trc_event_tp_set_p set)
{
    trc_thread_info_p new;
    int               pe;
    int               thr;
 
    pe = thread_id.my_pid;
    thr = thread_id.my_thread;
    if (thr < 0 || thr >= trc->thread[pe].n_threads) {
	fprintf(stderr, "Error: add thread id %d (max %d)\n",
		thr, trc->thread[pe].n_threads);
	return;
    }
 
    new = &trc->thread[pe].threads[thr];
    new->name = strdup(name);
    new->t_fix_current = trc->t_start;
    new->thread_id = thread_id;
    new->entries = entries;
    new->event_tp_set = set;

    if (thr == trc->thread[pe].n_threads - 1) {
	new->next_thread = NULL;
    } else {
	new->next_thread = &trc->thread[pe].threads[thr + 1];
    }
}


static boolean
trc_block_read_upto_hdr(trc_p trc, trc_block_hdr_p hdr)
{
    block_tp_t  block_tp;
    char        pre_hdr[sizeof(block_tp_t)];
    FILE       *s;

    s = trc->in_buf.stream;

    while (TRUE) {
	fread(pre_hdr, 1, sizeof(block_tp_t), s);
	if (feof(s)) {
	    return FALSE;
	}

	if (pre_hdr[0]) {
	    trc->in_buf.format |= LITTLE_ENDIAN;
	}
	if (pre_hdr[1]) {
	    trc->in_buf.format |= COMPACT_FORMAT;
	}
	block_tp = (block_tp_t)pre_hdr[sizeof(block_tp_t) - 1];

	switch (block_tp) {
	case TRC_FMT_BLOCK:
	    trc_state_skip(trc);
	    break;
	case TRC_DATA_BLOCK:
	    trc_block_hdr_read(trc, hdr);
	    return TRUE;
	default:
	    pan_panic("Illegal trace file block\n");
	    exit(10);
	}
    }
}


char *
trc_block_read(trc_p trc, trc_block_hdr_p hdr)
{
    FILE       *s;
    char       *p;

    s = trc->in_buf.stream;

    if (trc_block_read_upto_hdr(trc, hdr)) {
	if (hdr->size == 0) {
	    p = dummy_str;
	} else {
	    p = pan_malloc(hdr->size);
	    fread(p, 1, hdr->size, s);
	}
    } else {
	p = NULL;
    }
    return p;
}


char *
trc_block_thread_read(trc_p trc, trc_thread_id_t thread_id, size_t *size,
		      long int *file_pos)
{
    trc_block_hdr_t hdr;
    char       *p;
    FILE       *s = trc->in_buf.stream;

    fseek(s, *file_pos, 0);
    while (TRUE) {
	if (trc_block_read_upto_hdr(trc, &hdr)) {
	    if (trc_thread_id_eq(hdr.thread_id, thread_id)) {
		if (hdr.size != 0) {
		    p = pan_malloc(hdr.size);
		    fread(p, 1, hdr.size, s);
		    *size = hdr.size;
		    *file_pos = ftell(s);
		    return p;
		}
	    } else {
		if (fseek(s, hdr.size, 1) == -1) {
		    pan_panic("seek of data buf failed\n");
		    return NULL;
		}
	    }
	} else {
	    *size = 0;
	    return NULL;
	}
    }
    return NULL;	/*NOTREACHED*/
}


void
trc_block_hdr_write(trc_p trc, size_t size, trc_thread_id_t thread_id)
{
    char        pre_hdr[sizeof(block_tp_t)];
    FILE       *s;

    s = trc->out_buf.stream;
    pre_hdr[0] = IS_LITTLE_ENDIAN(trc->out_buf.format);
    pre_hdr[1] = IS_COMPACT_FORMAT(trc->out_buf.format);
    pre_hdr[sizeof(block_tp_t) - 1] = (char)TRC_DATA_BLOCK;
    fwrite(pre_hdr, 1, sizeof(block_tp_t), s);

    fwrite((char *)&size, sizeof(size_t), 1, s);
    fwrite((char *)&thread_id, sizeof(trc_thread_id_t), 1, s);
}


boolean
trc_open_infile(trc_p trc, char *in_filename)
{
    if (in_filename != NULL && strcmp(in_filename, "-") != 0) {
	trc->in_buf.stream = fopen(in_filename, "r");
	if (trc->in_buf.stream == NULL) {
	    return FALSE;
	}
    } else {
	trc->in_buf.stream = stdin;
    }
    trc->in_buf.buf = NULL;
    trc->in_buf.current = NULL;
    trc->in_buf.size = 0;

    trc->in_buf.t_current.t_sec = 0;
    trc->in_buf.t_current.t_nsec = 0;

    trc->in_buf.format = 0;

    return TRUE;
}


boolean
trc_open_outfile(trc_p trc, char *out_filename, size_t out_buf_size,
		 trc_thread_id_t thread_id)
{
    if (out_filename != NULL && strcmp(out_filename, "-") != 0) {
	trc->out_buf.stream = fopen(out_filename, "w");
	if (trc->out_buf.stream == NULL) {
	    return FALSE;
	}
    } else {
	trc->out_buf.stream = stdout;
    }
    trc->out_buf.thread_id = thread_id;
    if (out_buf_size == 0) {
	trc->out_buf.buf = NULL;
	trc->out_buf.current = NULL;
	trc->out_buf.size = 0;
    } else {
	trc->out_buf.buf = pan_malloc(out_buf_size);
	trc->out_buf.current = trc->out_buf.buf;
	trc->out_buf.size = out_buf_size;
    }

    trc->out_buf.t_current.t_sec = 0;
    trc->out_buf.t_current.t_nsec = 0;

    trc->out_buf.format |= COMPACT_FORMAT;

    return TRUE;
}


/****************************************************************************/
/* trc_read_next_event: read next event from file. May have to switch to    */
/*                  another data block in the file.                         */
/****************************************************************************/
boolean
trc_read_next_event(trc_p trc, trc_event_descr_p e)
{
    trc_lst_p   buf;
    trc_block_hdr_t hdr;

    buf = &trc->in_buf;
    do {
	if (buf->current - buf->buf == buf->size) {
					/* Data block exhausted. Get a new one.
					 * May have to skip fmt block. */

	    if (!trc_block_read_upto_hdr(trc, &hdr)) {
					/* No more data blocks: finished. */
		return FALSE;
	    }

	    buf->buf = NULL;
	    buf->current = buf->buf;
	    buf->size = hdr.size;
	}

	buf->current += trc_event_read(trc, e);

    } while (e->type == CLOCK_SHIFT || e->type == EMB_CLOCK_SHIFT);

    return TRUE;			/*NOTREACHED*/
}


static void
trc_block_write(trc_p trc)
{
    int         bytes;
    trc_lst_p   buf;

    buf = &trc->out_buf;
    bytes = buf->current - buf->buf;
    trc_block_hdr_write(trc, bytes, buf->thread_id);
    fwrite(buf->buf, 1, bytes, buf->stream);

    buf->current = buf->buf;
}


void
trc_write_next_event(trc_p trc, trc_event_descr_p e)
{
    trc_lst_p   buf;
    size_t      event_size;
    trc_compact_t ec;
    pan_time_fix_t dt;

    if (IS_IMPLICIT_SRC(trc->out_buf.format)) {
	fprintf(stderr,
		"trc_write_next_event of IMPLICIT_SRC mode not supported\n");
	exit(21);
    }

    buf = &trc->out_buf;
    if (buf->size == 0) {
	trc_event_write(trc, e);
    } else {

	dt = e->t;
	pan_time_fix_sub(&dt, &buf->t_current);
	buf->t_current = e->t;

	if (dt.t_sec != 0) {		/* Write an EMB_CLOCK_SHIFT */
	    assert(buf->buf + buf->size >= buf->current);

	    event_size = sizeof(trc_compact_t);
	    if (buf->current + event_size > buf->buf + buf->size) {
		trc_block_write(trc);
		buf->current = buf->buf;
	    }

	    ec.type = EMB_CLOCK_SHIFT;
	    ec.datum.t_clock_shift = e->t;

	    memcpy(buf->current, &ec, sizeof(trc_compact_t));
	    buf->current += sizeof(trc_compact_t);

	    dt.t_nsec = 0;
	}

	assert(buf->buf + buf->size >= buf->current);

	event_size = sizeof(trc_compact_t) + e->usr_size;
	if (buf->current + event_size > buf->buf + buf->size) {
	    trc_block_write(trc);
	    buf->current = buf->buf;
	}

	ec.type = e->type;
	ec.datum.ev_datum.dt        = dt.t_nsec;
	ec.datum.ev_datum.thread_id = e->thread_id;

	memcpy(buf->current, &ec, sizeof(trc_compact_t));
	buf->current += sizeof(trc_compact_t);
	memcpy(buf->current, e->usr, e->usr_size);
	buf->current += e->usr_size;

	assert(buf->buf + buf->size >= buf->current);
    }
}


void
trc_close_infile(trc_p trc)
{
    if (trc->in_buf.stream != stdin &&
	    trc->in_buf.stream != NULL) {
	fclose(trc->in_buf.stream);
    }
    if (trc->in_buf.buf != NULL) {
	pan_free(trc->in_buf.buf);
    }
}


void
trc_close_outfile(trc_p trc)
{
    if (trc->out_buf.buf != NULL) {
	if (trc->out_buf.current > trc->out_buf.buf) {
	    trc_block_write(trc);
	}
	pan_free(trc->out_buf.buf);
    }
    if (trc->out_buf.stream != stdout &&
	    trc->out_buf.stream != NULL) {
	fclose(trc->out_buf.stream);
    }
}


void
trc_close_files(trc_p trc)
{
    trc_close_infile(trc);
    trc_close_outfile(trc);
}
