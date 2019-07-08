/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#ifndef __CONN_TABLE_H__ 
#define __CONN_TABLE_H__

#include "pan_sys.h"
#include "pan_mp_types.h"


/*
 * This module defines the interface to the connection table package.
 * This table remembers the last sequence number seen from a given sender
 * and returns whether it is a duplicate or not. It also hands out
 * sequence numbers for a given destination. The user of a sequence
 * number must guarantee that the other site will receive a message with
 * this sequence number.
 */


/* Possible return values of conn_check_message() */
#define CONN_DUPLICATE          1
#define CONN_SEQNO_OVERRUN      2
#define CONN_OK                 3

extern void pan_mp_conn_start(void);
extern void pan_mp_conn_end(void);
extern void pan_mp_conn_create(int nr_threads);
extern void pan_mp_conn_destroy(void);
extern int pan_mp_conn_check_message(int cpu, seqno_t seqno);
extern void pan_mp_conn_register_message(int cpu, seqno_t seqno);
extern seqno_t pan_mp_conn_get_seqno(int cpu);


#endif /* __CONN_TABLE_H__ */



