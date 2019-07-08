/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef _TRACE_TRC_IO_H
#define _TRACE_TRC_IO_H


#include <stddef.h>
#include <stdio.h>

#include "pan_sys.h"

#ifndef TRACING
#define TRACING
#endif
#include "pan_trace.h"

#include "trc_types.h"



typedef struct TRC_BLOCK_HDR_T trc_block_hdr_t, *trc_block_hdr_p;

struct TRC_BLOCK_HDR_T {
    size_t          size;
    trc_thread_id_t thread_id;
};


typedef enum TRC_OUTPUT_T {
    TRC_OUTPUT_ASCII,
    TRC_OUTPUT_UPSHOT,
    TRC_OUTPUT_PICL,
    TRC_OUTPUT_XAB
}           trc_output_t, *trc_output_p;


extern const char trc_marker[];

int         trc_check_int(char *fmt, int *count);

char       *trc_block_thread_read(trc_p trc, trc_thread_id_t thread_id,
				  size_t *size, long int *file_pos);

char       *trc_block_read(trc_p trc, trc_block_hdr_p hdr);

void        trc_block_hdr_write(trc_p trc, size_t size,
				trc_thread_id_t thread_id);

size_t      trc_event_get(char *buf, trc_p trace_state, trc_event_descr_p e);

boolean     trc_open_infile(trc_p trace_state, char *in_filename);

boolean     trc_open_outfile(trc_p trace_state, char *out_filename,
			     size_t out_buf_size, trc_thread_id_t thread_id);

void        trc_close_infile(trc_p trace_state);

void        trc_close_outfile(trc_p trace_state);

void        trc_close_files(trc_p trace_state);

boolean     trc_read_next_event(trc_p trace_state, trc_event_descr_p e);

void        trc_write_next_event(trc_p trace_state, trc_event_descr_p e);

void        trc_add_thread(trc_p trace_state, trc_thread_id_t thread_id, 
			   int entries, char *name, trc_event_tp_set_p set);

boolean     trc_state_read(trc_p trace_state);

void        trc_state_write(trc_p trace_state);

void        trc_state_clear(trc_p trc_state);

void        trc_event_lst_write(FILE *s, trc_event_lst_p event_list);

#endif
