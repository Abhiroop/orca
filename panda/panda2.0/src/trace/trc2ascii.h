/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

/* Function prototypes for converting trace format to human-readable format
 *
 * Author: Rutger Hofman, VU Amsterdam, november 1993.
 */

#ifndef TRACE_TRC2ASCII_H
#define TRACE_TRC2ASCII_H

#include <stddef.h>
#include <stdio.h>

#include "pan_sys.h"

#include "trc_types.h"



void trc_event_printf(FILE *s, trc_event_lst_p event_list,
		      trc_event_descr_p e, pan_time_fix_p t_offset);

void trc_printf_usr(FILE *s, char *usr, char *fmt);

void trc_sprintf_usr(char *s, char *usr, char *fmt);

void trc_block_hdr_printf(FILE *s, size_t size, char *thread_name,
			  trc_thread_id_t thread_id);

void trc_fmt_printf(FILE *s, trc_p trace_state);

int  trc_fmt_length(char *fmt);

int  trc_get_one_tok(char *buf, char *fmt, void *val, int n_tok);

int  trc_print_tok(FILE *s, char *fmt, size_t *n_fmt, char *buf,
		   size_t *n_buf);

int  trc_print_sel_tok(FILE *s, char *fmt_chars, char *fmt, size_t *n_fmt,
		       char *buf, size_t *n_buf);

int  trc_sprint_tok(char *s, char *fmt, size_t *n_fmt, char *buf,
		    size_t *n_buf);

int  trc_sprint_sel_tok(char *s, char *fmt_chars, char *fmt, size_t *n_fmt,
		        char *buf, size_t *n_buf);

void trc_event_lst_printf(FILE *s, trc_event_lst_p lst);

#endif
