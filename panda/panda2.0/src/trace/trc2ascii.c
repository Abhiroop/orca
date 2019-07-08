/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stddef.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#include <limits.h>
#include <float.h>

#include "pan_sys.h"

#ifndef TRACING
#define TRACING
#endif

#include "pan_trace.h"

#include "pan_util.h"

#include "trc_types.h"
#include "trc_event_tp.h"
#include "trc_align.h"
#include "trc_io.h"
#include "trc2ascii.h"


void
trc_event_printf(FILE *s, trc_event_lst_p event_list, trc_event_descr_p e,
		 pan_time_fix_p t_offset)
{
    char       *name;
    char       *fmt;
    pan_time_fix_t    dt;
    trc_event_t extern_id;

    trc_event_lst_query_extern(event_list, e->type, &extern_id, &name, &fmt);
    fprintf(s, "%-20s ", name);
    fprintf(s, "%4d %4d ", e->thread_id.my_pid, e->thread_id.my_thread);
    dt = e->t;
    pan_time_fix_sub(&dt, t_offset);
    fprintf(s, "%-8f ", pan_time_fix_t2d(&dt));
    trc_printf_usr(s, e->usr, fmt);
    fprintf(s, "\n");
}


void
trc_printf_usr(FILE *s, char *usr, char *fmt)
{
    size_t      n_fmt;
    size_t      n_usr;

    n_fmt = 0;
    n_usr = 0;
    do {
	trc_print_tok(s, fmt, &n_fmt, usr, &n_usr);
    } while (fmt[n_fmt] != '\0');
}


void
trc_sprintf_usr(char *s, char *usr, char *fmt)
{
    size_t      n_fmt;
    size_t      n_usr;

    n_fmt = 0;
    n_usr = 0;
    do {
	trc_sprint_tok(s, fmt, &n_fmt, usr, &n_usr);
	s = strchr(s, '\0');
    } while (fmt[n_fmt] != '\0');
}


void
trc_block_hdr_printf(FILE *s, size_t size, char *thread_name,
		     trc_thread_id_t thread_id)
{
    fprintf(s, "%ld bytes written by thread \"%s\" (%d)at pe %d\n",
	 (long int)size, thread_name, thread_id.my_thread, thread_id.my_pid);
}


static time_t
pan_time_fix_t2tm(pan_time_fix_p t)
{
    return (time_t)t->t_sec;
}


void
trc_fmt_printf(FILE *s, trc_p trc)
{
    trc_thread_info_p p;
    time_t            tc;
    int               pe;
    int               space;

    tc = pan_time_fix_t2tm(&trc->t_start);
    fprintf(s, "Trace of \"%s\" at %s",
	    trc->filename, ctime(&tc));
    fprintf(s, "Platform %d of %d\n",
	    trc->my_pid, trc->n_pids);
    fprintf(s, "%-20s : %f +- %f\n", "Trace synch offset",
	    pan_time_fix_t2d(&trc->t_off),
	    pan_time_fix_t2d(&trc->t_d_off));
    fprintf(s, "%-20s : %f\n", "Start trace",
		pan_time_fix_t2d(&trc->t_start));
    fprintf(s, "%-20s : %f\n", "Stop trace",
		pan_time_fix_t2d(&trc->t_stop));
    fprintf(s, "%6d trc events\n", trc->entries);
    fprintf(s, "%6d threads in trace use\n", trc->n_threads);
    for (pe = -1; pe < trc->n_pids + PANDA_1; pe++) {
	p = trc->thread[pe].threads;
	while (p != NULL) {
	    fprintf(s, "thread \"%s\"", p->name);
	    space = 30 - strlen(p->name);
	    fprintf(s, "%*s", space, "");
	    fprintf(s, " %3d at platform %3d: %4d events\n",
		    p->thread_id.my_thread, pe, p->entries);
	    p = p->next_thread;
	}
    }
}


static void
trc_skip_pref(char *fmt, size_t *n_fmt)
{
    char       *fmt_p;

    fmt_p = fmt + *n_fmt;
    while (TRUE) {
	while (*fmt_p != '\0' && *fmt_p != '%')
	    ++fmt_p;
	if (fmt_p[0] != '\0' && fmt_p[1] == '%') {
	    fmt_p += 2;
	} else {
	    *n_fmt = fmt_p - fmt;
	    return;
	}
    }
}


static void
trc_gen_print_pref(FILE *f, char *s, char *fmt, size_t *n_fmt)
{
    char       *fmt_p;
    char       *perc;

    fmt_p = fmt + *n_fmt;
    perc = fmt_p;
    do {
	while (*perc != '\0' && *perc != '%')
	    ++perc;
	if (f != NULL)
	    fprintf(f, "%.*s", perc - fmt_p, fmt_p);
	if (s != NULL) {
	    sprintf(s, "%.*s", perc - fmt_p, fmt_p);
	    s = strchr(s, '\0');
	}
	fmt_p = perc;
	if (fmt_p[0] != '\0' && fmt_p[1] == '%') {
	    if (f != NULL)
		fprintf(f, "%%");
	    if (s != NULL) {
		sprintf(s, "%%");
		s = strchr(s, '\0');
	    }
	    fmt_p += 2;
	    perc = fmt_p;
	} else {
	    *n_fmt = fmt_p - fmt;
	    return;
	}
    } while (TRUE);
}


static int
trc_pref_length(char *fmt, size_t *n_fmt)
{
    char       *fmt_p;
    int         n;

    fmt_p = fmt + *n_fmt;
    n = 0;
    do {
	while (*fmt_p != '\0' && *fmt_p != '%') {
	    ++fmt_p;
	    ++n;
	}
	if (fmt_p[0] != '\0' && fmt_p[1] == '%') {
	    fmt_p += 2;
	    ++n;
	} else {
	    *n_fmt = fmt_p - fmt;
	    return n;
	}
    } while (TRUE);
    return -1;			/* Not reached */
}


static int
trc_tok_length(char fmt_c)
{
    int         n;
    static boolean inited = FALSE;
    static int  double_width;
    static int  int_width;
    static int  ptr_width;
    static int  short_width;
    static int  long_width;
    static int  char_width;
    static int  byte_width;

    if (!inited) {
	double_width = 2 + DBL_DIG;	/* sign + dot */
	int_width = 1;		/* sign */
	n = INT_MAX;
	while (n > 1) {
	    ++int_width;
	    n /= 10;
	}
	ptr_width = (sizeof(void *) + 3) / 4;	/* 4 bits is 1 hex digit */
	short_width = 1;		/* sign */
	n = SHRT_MAX;
	while (n > 1) {
	    ++short_width;
	    n /= 10;
	}
	long_width = 1;		/* sign */
	n = LONG_MAX;
	while (n > 1) {
	    ++long_width;
	    n /= 10;
	}
	char_width = 1;
	byte_width = 1;
	inited = TRUE;
    }
    switch (fmt_c) {
    case 'f':
	n = double_width;
	break;
    case 'd':
	n = int_width;
	break;
    case 'p':
	n = ptr_width;
	break;
    case 'h':
	n = short_width;
	break;
    case 'l':
	n = long_width;
	break;
    case 'b':
	n = byte_width;
	break;
    case 'c':
	n = char_width;
	break;
    default:
	fprintf(stderr, "%%%c conversion not supported\n",
		fmt_c);
	n = -1;
    }
    return n;
}



int
trc_fmt_length(char *fmt)
{
    int         len;
    size_t      n_fmt;
    char       *fmt_p;
    int         count;
    int         n;

    n_fmt = 0;
    fmt_p = fmt;
    len = 0;
    while (fmt_p[0] != '\0') {
	len += trc_pref_length(fmt, &n_fmt);
	fmt_p = fmt + n_fmt;
	if (fmt_p[0] == '\0') {
	    ++len;
	    return len;
	}
	++fmt_p;		/* Skip the '%' */
	n = trc_check_int(fmt_p, &count);
	if (n != 0) {
	    fmt_p += n;		/* Skip the digits */
	} else {
	    count = 1;
	}
	if (fmt_p[0] == 's') {
	    len += count;
	} else {
	    len += count * trc_tok_length(fmt_p[0]);
	}
	++fmt_p;		/* Skip the format char */
	n_fmt = fmt_p - fmt;
    }
    ++len;			/* The '\0' char */
    return len;
}



static void
print_one_tok(FILE *f, char *s, char fmt_c, char *usr, size_t *n_usr)
{
    char       *usr_p;

    usr_p = usr + *n_usr;
    switch (fmt_c) {
    case 'f':
	usr_p = ptr_align(usr_p, sizeof(double));
	if (f != NULL)
	    fprintf(f, "%f", *((double *)usr_p));
	if (s != NULL) {
	    sprintf(s, "%f", *((double *)usr_p));
	    s = strchr(s, '\0');
	}
	usr_p += sizeof(double);
	break;
    case 'd':
	usr_p = ptr_align(usr_p, sizeof(int));
	if (f != NULL)
	    fprintf(f, "%d", *((int *)usr_p));
	if (s != NULL) {
	    sprintf(s, "%d", *((int *)usr_p));
	    s = strchr(s, '\0');
	}
	usr_p += sizeof(int);
	break;
    case 'p':
	usr_p = ptr_align(usr_p, sizeof(void *));
	if (f != NULL)
	    fprintf(f, "%p", *((void **)usr_p));
	if (s != NULL) {
	    sprintf(s, "%p", *((void **)usr_p));
	    s = strchr(s, '\0');
	}
	usr_p += sizeof(void *);
	break;
    case 'h':
	usr_p = ptr_align(usr_p, sizeof(short int));
	if (f != NULL)
	    fprintf(f, "%hd", *((short int *)usr_p));
	if (s != NULL) {
	    sprintf(s, "%hd", *((short int *)usr_p));
	    s = strchr(s, '\0');
	}
	usr_p += sizeof(short int);
	break;
    case 'l':
	usr_p = ptr_align(usr_p, sizeof(long int));
	if (f != NULL)
	    fprintf(f, "%ld", *((long int *)usr_p));
	if (s != NULL) {
	    sprintf(s, "%ld", *((long int *)usr_p));
	    s = strchr(s, '\0');
	}
	usr_p += sizeof(long int);
	break;
    case 'b':
	usr_p = ptr_align(usr_p, 1);
	if (f != NULL)
	    fprintf(f, "%d", (int) *usr_p);
	if (s != NULL) {
	    sprintf(s, "%d", (int) *usr_p);
	    s = strchr(s, '\0');
	}
	usr_p += 1;
	break;
    case 'c':
	usr_p = ptr_align(usr_p, sizeof(char));
	if (f != NULL)
	    fprintf(f, "%c", *usr_p);
	if (s != NULL) {
	    sprintf(s, "%c", *usr_p);
	    s = strchr(s, '\0');
	}
	usr_p += sizeof(char);
	break;
    default:
	fprintf(stderr, "%%%c conversion not supported\n",
		fmt_c);
    }
    *n_usr = usr_p - usr;
}


#define skip_tok(p, tp) \
		(ptr_align(p, sizeof(tp)) + sizeof(tp))


static void
skip_one_tok(char fmt_c, char *usr, size_t *n_usr)
{
    char       *usr_p;

    usr_p = usr + *n_usr;
    switch (fmt_c) {
    case 'f':
	usr_p = skip_tok(usr_p, double);
	break;
    case 'd':
	usr_p = skip_tok(usr_p, int);
	break;
    case 'p':
	usr_p = skip_tok(usr_p, void *);
	break;
    case 'h':
	usr_p = skip_tok(usr_p, short int);
	break;
    case 'l':
	usr_p = skip_tok(usr_p, long int);
	break;
    case 'b':
	++usr_p;
	break;
    case 'c':
	++usr_p;
	break;
    default:
	fprintf(stderr, "%%%c conversion not supported\n",
		fmt_c);
    }
    *n_usr = usr_p - usr;
}


static int
trc_gen_print_tok(FILE *f, char *s, char *fmt, size_t *n_fmt, char *usr,
		  size_t *n_usr)
{
    char        fmt_c;
    char       *fmt_p;
    char       *usr_p;
    int         n;
    int         count;

    trc_gen_print_pref(f, s, fmt, n_fmt);
    if (s != NULL)
	s = strchr(s, '\0');
    fmt_p = fmt + *n_fmt;
    if (fmt_p[0] == '\0') {
	*n_usr = 0;
	*n_fmt = fmt_p - fmt;
	return 0;
    }
    ++fmt_p;			/* Skip the '%' */
    n = trc_check_int(fmt_p, &count);
    if (n != 0) {
	fmt_p += n;		/* Skip the digits */
    } else {
	count = 1;
    }
    fmt_c = fmt_p[0];
    ++fmt_p;			/* Skip the format char */
    *n_fmt = fmt_p - fmt;
    if (fmt_c == 's') {
	usr_p = usr + *n_usr;
	usr_p = ptr_align(usr_p, sizeof(char));
	if (f != NULL)
	    fprintf(f, "%s", usr_p);
	if (s != NULL) {
	    sprintf(s, "%s", usr_p);
	    s = strchr(s, '\0');
	}
	usr_p += count * sizeof(char);
	*n_usr = usr_p - usr;
    } else {
	for (n = 0; n < count; n++) {
	    print_one_tok(f, s, fmt_c, usr, n_usr);
	    if (s != NULL)
		s = strchr(s, '\0');
	}
    }
    return 1;
}


int
trc_print_tok(FILE *s, char *fmt, size_t *n_fmt, char *usr, size_t *n_usr)
{
    trc_gen_print_tok(s, NULL, fmt, n_fmt, usr, n_usr);
    return 1;
}


int
trc_sprint_tok(char *s, char *fmt, size_t *n_fmt, char *usr, size_t *n_usr)
{
    trc_gen_print_tok(NULL, s, fmt, n_fmt, usr, n_usr);
    return 1;
}


static int
trc_print_gen_sel_tok(FILE *f, char *s, char *fmt_chars, char *fmt,
		      size_t *n_fmt, char *usr, size_t *n_usr)
{
    char        fmt_c;
    char       *fmt_p;		/* = &fmt[*n_fmt] */
    char       *usr_p;		/* = &usr[*n_usr] */
    int         n;
    boolean     found = FALSE;	/* defy compiler warning */
    int         count;

    do {
	trc_gen_print_pref(NULL, NULL, fmt, n_fmt);
	if (s != NULL)
	    s = strchr(s, '\0');
	fmt_p = fmt + *n_fmt;
	if (fmt_p[0] == '\0') {
	    *n_usr = 0;
	    *n_fmt = fmt_p - fmt;
	    return 0;
	}
	++fmt_p;		/* Skip the '%' */
	n = trc_check_int(fmt_p, &count);
	if (n != 0) {
	    fmt_p += n;		/* Skip the digits */
	} else {
	    count = 1;
	}
	fmt_c = fmt_p[0];
	++fmt_p;		/* Skip the format char */
	*n_fmt = fmt_p - fmt;
	if (fmt_c == 's') {
	    usr_p = usr + *n_usr;
	    usr_p = ptr_align(usr_p, sizeof(char));
	    if (strchr(fmt_chars, fmt_c) != NULL) {
		if (f != NULL)
		    fprintf(f, "%s", usr_p);
		if (s != NULL) {
		    sprintf(s, "%s", usr_p);
		    s = strchr(s, '\0');
		}
	    }
	    usr_p += count * sizeof(char);
	    *n_usr = usr_p - usr;
	} else {
	    for (n = 0; n < count; n++) {
		if (strchr(fmt_chars, fmt_c) != NULL) {
		    print_one_tok(f, s, fmt_c, usr, n_usr);
		    found = TRUE;
		} else {
		    skip_one_tok(fmt_c, usr, n_usr);
		}
		if (s != NULL)
		    s = strchr(s, '\0');
	    }
	}
    } while (!found);
    *n_fmt = fmt_p - fmt;
    return 1;
}


int
trc_print_sel_tok(FILE *s, char *fmt_chars, char *fmt, size_t *n_fmt,
		  char *usr, size_t *n_usr)
{
    trc_print_gen_sel_tok(s, NULL, fmt_chars, fmt, n_fmt, usr, n_usr);
    return 1;
}


int
trc_sprint_sel_tok(char *s, char *fmt_chars, char *fmt, size_t *n_fmt,
		   char *usr, size_t *n_usr)
{
    trc_print_gen_sel_tok(NULL, s, fmt_chars, fmt, n_fmt, usr, n_usr);
    return 1;
}


int
trc_get_one_tok(char *usr, char *fmt, void *val, int n_tok)
{
    int         i;
    int         n;
    int         count;
    char       *fmt_p;
    char       *usr_p;
    size_t      n_fmt;
    size_t      n_usr;
    char        fmt_c;

    usr_p = usr;
    n_fmt = 0;
    n_usr = 0;
    for (i = 0; i <= n_tok; i++) {
	trc_skip_pref(fmt, &n_fmt);	/* Skip text */
	fmt_p = fmt + n_fmt;
	if (fmt_p[0] == '\0') {
	    return 0;
	}
	++fmt_p;		/* Skip the '%' */
	n = trc_check_int(fmt_p, &count);
	if (n != 0) {
	    fmt_p += n;		/* Skip the digits */
	} else {
	    count = 1;
	}
	fmt_c = fmt_p[0];
	if (i == n_tok) {
	    switch (fmt_c) {
	    case 'f':
		usr_p = ptr_align(usr_p, sizeof(double));
		memcpy(val, usr_p, sizeof(double));
		break;
	    case 'd':
		usr_p = ptr_align(usr_p, sizeof(int));
		memcpy(val, usr_p, sizeof(int));
		break;
	    case 'p':
		usr_p = ptr_align(usr_p, sizeof(void *));
		memcpy(val, usr_p, sizeof(void *));
		break;
	    case 'h':
		usr_p = ptr_align(usr_p, sizeof(short int));
		memcpy(val, usr_p, sizeof(short int));
		break;
	    case 'l':
		usr_p = ptr_align(usr_p, sizeof(long int));
		memcpy(val, usr_p, sizeof(long int));
		break;
	    case 'b':
		usr_p = ptr_align(usr_p, 1);
		memcpy(val, usr_p, 1);
		break;
	    case 'c':
		usr_p = ptr_align(usr_p, sizeof(char));
		memcpy(val, usr_p, sizeof(char));
		break;
	    case 's':
		usr_p = ptr_align(usr_p, sizeof(char));
		strncpy(val, usr_p, count);
		break;
	    default:
		fprintf(stderr, "%%%c conversion not supported\n",
			fmt_c);
	    }
	} else {
	    ++fmt_p;		/* Skip the format char */
	    n_fmt = fmt_p - fmt;
	    if (fmt_c == 's') {
		usr_p = usr + n_usr;
		usr_p = ptr_align(usr_p, sizeof(char));
		usr_p += count * sizeof(char);
		n_usr = usr_p - usr;
	    } else {
		for (n = 0; n < count; n++) {
		    skip_one_tok(fmt_c, usr, &n_usr);
		}
		usr_p = &usr[n_usr];
	    }
	}
    }
    return 1;
}


void
trc_event_lst_printf(FILE *s, trc_event_lst_p lst)
{
    trc_event_t e;
    size_t      usr_size;
    int         level;
    trc_event_t e_extern;
    char       *name;
    char       *fmt;

    fprintf(s, "%d event bindings:\n%s %s %s %s %s %s\n",
	    trc_event_lst_num(lst), "<event id>", "<extern id>", "<level>",
	    "<usr data size>", "<event name>", "\"format\"");
    e = trc_event_lst_first(lst);
    while (e != TRC_NO_SUCH_EVENT) {
	trc_event_lst_query(lst, e, &usr_size, &level);
	trc_event_lst_query_extern(lst, e, &e_extern, &name, &fmt);
	fprintf(s, "%3d %3d %5d %3ld %-20s \"%s\"\n",
		e, e_extern, level, (long int)usr_size, name, fmt);
	e = trc_event_lst_next(lst, e);
    }
}
