/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_index.h"
#include "pan_bg_global.h"

#define NR_INDEX          512 /* must be a multiple of 8 */
#define BITMASK_SIZE      (NR_INDEX / 8)

/*$typedef unsigned char byte_t;$*/

typedef struct {
    seqno_t       minimum;
    unsigned char bitmask[BITMASK_SIZE];
} index_table_t, *index_table_p;

static seqno_t       index;
static index_table_p index_table;

#define SETBIT(byte, bit)    byte |= (unsigned char)(1 << (bit))
#define TESTBIT(byte, bit)   ((byte) & (unsigned char)(1 << (bit)))
#define CLEARBIT(byte, bit)  byte &= (unsigned char)(~(1 << (bit)))

#define MOD(x) ((x) % NR_INDEX)

#define test_bit(mask, index) \
  TESTBIT((mask)[MOD(index) / 8], (MOD(index) % 8))
#define set_bit(mask, index) \
  SETBIT((mask)[MOD(index) / 8], (MOD(index) % 8))
#define clear_bit(mask, index) \
  CLEARBIT((mask)[MOD(index) / 8], (MOD(index) % 8))


/* Interface routines */

void 
index_start(void)
{
    index_table = pan_calloc(pan_nr_platforms(), sizeof(index_table_t));
}

void
index_end(void)
{
    pan_free(index_table);
}    

/*
 * index_check:
 *                 Checks whether a fragment from pid has already been
 *                 received.
 */

int 
index_check(seqno_t index, int pid)
{
    int ret;

    assert((pid >= 0) && (pid < pan_nr_platforms()));
    
    if (index < index_table[pid].minimum){
	ret = INDEX_DUPLICATE;
    }else if (index - index_table[pid].minimum >= NR_INDEX){
	ret = INDEX_OVERRUN;
    }else if (test_bit(index_table[pid].bitmask, index)){
	ret = INDEX_DUPLICATE;
    }else {
	ret = INDEX_OK;
    }
    
    return ret;
}

/*
 * index_set:
 *                 Sets the receipt of a fragment with local index index
 *                 from pid.
 */

void 
index_set(seqno_t index, int pid)
{
    int i;

    assert((pid >= 0) && (pid < pan_nr_platforms()));

    assert (index >= index_table[pid].minimum);
    assert (index - index_table[pid].minimum < NR_INDEX);
    assert (!test_bit(index_table[pid].bitmask, index));

    set_bit(index_table[pid].bitmask, index);
    if (index == index_table[pid].minimum){
	for (i = index; test_bit(index_table[pid].bitmask, i); i++){
	    clear_bit(index_table[pid].bitmask, i);
	}
	index_table[pid].minimum = i;
    }
}

/*
 * index_get:
 *                 Get a local index number. The numbers are guaranteed
 *                 to be consecutive.
 */

seqno_t
index_get(void)
{
    return(index++);
}









