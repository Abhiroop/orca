/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_sys.h"
#include "pan_mp_types.h"
#include "pan_mp_conn.h"
#include "pan_mp_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define BITMASK_SIZE      (CONN_NR_SEQNO / 8)

				/* Types */
typedef struct {
    seqno_t          minimum;
    unsigned char    bitmask[BITMASK_SIZE];
} conn_table_t, *conn_table_p;


				/* Local variables */
static conn_table_p    conn_table;
static seqno_t        *seqno_table;
static int             nr_platforms;

				/* Macros */
#define SETBIT(byte, bit)    byte |= (unsigned char)(1 << (bit))
#define TESTBIT(byte, bit)   ((byte) & (unsigned char)(1 << (bit)))
#define CLEARBIT(byte, bit)  byte &= (unsigned char)(~(1 << (bit)))

#define MOD(x) ((x) % CONN_NR_SEQNO)

#define test_bit(mask, seqno) \
  TESTBIT((mask)[MOD(seqno) / 8], ((seqno) % 8))
#define set_bit(mask, seqno) \
  SETBIT((mask)[MOD(seqno) / 8], ((seqno) % 8))
#define clear_bit(mask, seqno) \
  CLEARBIT((mask)[MOD(seqno) / 8], ((seqno) % 8))


				/* Interface routines */

/*
 * pan_mp_conn_start:
 *                 Module initialization.
 */
void 
pan_mp_conn_start(void)
{
    nr_platforms = pan_nr_platforms();
    conn_table   = (conn_table_p)pan_calloc(nr_platforms, 
					    sizeof(conn_table_t));
    seqno_table  = (seqno_t *)pan_calloc(nr_platforms, sizeof(seqno_t));
}

/*
 * pan_mp_conn_end:
 *                 Module termination.
 */
void 
pan_mp_conn_end(void)
{
    pan_free(conn_table);
    pan_free(seqno_table);
}

/*
 * pan_mp_conn_check_message:
 *                 Check whether a given sequence number is:
 *                 - already registered (return DUPLICATE)
 *                 - too old to handle (return SEQNO_OVERRUN)
 *                 - OK otherwise
 */
int 
pan_mp_conn_check_message(int cpu, seqno_t seqno)
{
    conn_table_p cp;
    int ret;

    assert((cpu >= 0) && (cpu < nr_platforms));
    
    cp = &conn_table[cpu];
    if (seqno < cp->minimum){
	ret = CONN_DUPLICATE;
    }else if (seqno - cp->minimum >= CONN_NR_SEQNO){
	ret = CONN_SEQNO_OVERRUN;
    }else if (test_bit(cp->bitmask, seqno)){
	ret = CONN_DUPLICATE;
    }else{
	ret = CONN_OK;
    }

    return ret;
}

/*
 * pan_mp_conn_register_message:
 *                 Registers a received sequence number
 */
void 
pan_mp_conn_register_message(int cpu, seqno_t seqno)
{
    conn_table_p cp;
    int i;

    assert((cpu >= 0) && (cpu < nr_platforms));

    cp = &conn_table[cpu];

    assert (seqno >= cp->minimum);
    assert (seqno - cp->minimum < CONN_NR_SEQNO);
    assert (!test_bit(cp->bitmask, seqno));

    set_bit(cp->bitmask, seqno);
    if (seqno == cp->minimum){
	/* find next unset entry in bitmask */
	for (i = seqno; test_bit(cp->bitmask, i); i++){
	    clear_bit(cp->bitmask, i);
	}
	cp->minimum = i;
    }
}


/*
 * pan_mp_conn_get_seqno:
 *                 Hands out a sequence number for a given
 *                 destination. The sequence numbers are in order.
 */
seqno_t 
pan_mp_conn_get_seqno(int cpu)
{
    assert((cpu >= 0) && (cpu < nr_platforms));

    return seqno_table[cpu]++;
}






