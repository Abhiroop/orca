/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "pan_sys.h"

#include "pan_util.h"

#include "trc_types.h"
/* #include "trc_align.h" */
#include "trc_event_tp.h"




#define ULONG_BITS		(CHAR_BIT * sizeof(unsigned long int))

#define LOGULONGBITS		(5)		/* 2-base log of ULONG_BITS */
						/* assume ULONG_BITS == 32 */

#define ULONG_WORD(n)		((n) >> LOGULONGBITS)
#define ULONG_BIT(n)		((n) & (ULONG_BITS - 1))



const trc_event_t TRC_NO_SUCH_EVENT = -1;



void
trc_event_descr_init(trc_event_descr_p e)
{
    e->usr_size = 0;
}



/*ARGSUSED*/
void
trc_event_descr_clear(trc_event_descr_p e)
{
}


void
trc_event_descr_cpy(trc_event_descr_p from, trc_event_descr_p to)
{
    void       *p;

    if (to->usr_size == 0) {
	p = pan_malloc(from->usr_size);
    } else if (to->usr_size != from->usr_size) {
	p = pan_realloc(to->usr, from->usr_size);
    } else {
	p = to->usr;
    }
    *to = *from;
    to->usr = p;
    if (from->usr_size > 0) {
	memcpy(to->usr, from->usr, from->usr_size);
    }
}


boolean
trc_event_descr_equal(trc_event_descr_p e1, trc_event_descr_p e2)
{
    return (e1->type == e2->type && pan_time_fix_cmp(&e1->t, &e2->t) == 0 &&
	    memcmp(&e1->thread_id, &e2->thread_id,
		   sizeof(trc_thread_id_t)) == 0 &&
	    memcmp(e1->usr, e2->usr, e1->usr_size) == 0);
}



trc_event_lst_p
trc_event_lst_init(void)
{
    trc_event_lst_p e;
    trc_event_t i;

    e = pan_malloc(sizeof(trc_event_lst_t));
    e->num = 0;
    e->offset = 0;
    e->next = NULL;
    for (i = 0; i < TRC_EVENT_BLOCK; i++) {
	e->info[i].extern_id = TRC_NO_SUCH_EVENT;
    }
    return e;
}


static trc_event_info_p
trc_event_lst_locate(trc_event_lst_p block, trc_event_t e)
{
    while (e >= TRC_EVENT_BLOCK) {
	block = block->next;
	e -= TRC_EVENT_BLOCK;
    }
    return &block->info[e];
}


static trc_event_lst_p
new_block(trc_event_t offset)
{
    trc_event_lst_p new;
    trc_event_t i;

    new = pan_malloc(sizeof(trc_event_lst_t));
    new->offset = offset + TRC_EVENT_BLOCK;
    new->num = 0;
    new->next = NULL;
    for (i = 0; i < TRC_EVENT_BLOCK; i++) {
	new->info[i].extern_id = TRC_NO_SUCH_EVENT;
    }
    return new;
}


static trc_event_t
trc_event_lst_new(trc_event_lst_p block)
/* This function is NOT reentrant: multiple callers must put a lock
 * around it.
 * However, it is reentrant wrt trc_event_lst_query; queries between
 * calls of trc_event_lst_new and the corresponding trc_event_lst_bind
 * are illegal.
 */
{
    trc_event_t num;

    while (block->num == TRC_EVENT_BLOCK) {
	if (block->next == NULL) {
	    block->next = new_block(block->offset);
	}
	block = block->next;
    }
    num = block->offset + block->num;
    ++block->num;
    return num;
}


void
trc_event_lst_bind(trc_event_lst_p block, trc_event_t num, trc_event_t e_extern,
		   int level, size_t usr_size, char *name, char *fmt)
/* This function is NOT reentrant: multiple callers must put a lock
 * around it.
 * However, it is reentrant wrt trc_event_lst_query.
 */
{
    trc_event_info_p e;

    while (num >= TRC_EVENT_BLOCK) {
	if (block->next == NULL) {
	    block->next = new_block(block->offset);
	}
	block = block->next;
	num -= TRC_EVENT_BLOCK;
    }
    e = &block->info[num];
    e->level = level;
    e->usr_size = usr_size;
    e->extern_id = e_extern;
    e->name = strdup(name);
    e->fmt = strdup(fmt);
    if (block->num <= num) {
	block->num = num + 1;
    }
}


void
trc_event_lst_unbind(trc_event_lst_p block, trc_event_t num)
/* Unbind by writing an illegal value into the level field (-1) */
{
    trc_event_info_p e;

    e = trc_event_lst_locate(block, num);
    e->level = -1;
}


void
trc_event_lst_bind_extern(trc_event_lst_p block, trc_event_t num,
			  trc_event_t e_extern)
/* This function is NOT reentrant: multiple callers must put a lock
 * around it.
 * However, it is reentrant wrt trc_event_lst_query.
 */
{
    trc_event_info_p info;

    info = trc_event_lst_locate(block, num);
    info->extern_id = e_extern;
}


trc_event_t
trc_event_lst_add(trc_event_lst_p block, int level, size_t usr_size, char *name,
		  char *fmt)
/* This function is NOT reentrant: multiple callers must put a lock
 * around it.
 * However, it is reentrant wrt trc_event_lst_query.
 */
{
    trc_event_t e;

    e = trc_event_lst_new(block);
    trc_event_lst_bind(block, e, e, level, usr_size, name, fmt);
    return e;
}


trc_event_t
trc_event_lst_find(trc_event_lst_p block, char *name)
{
    trc_event_t i;

    while (block != NULL) {
	for (i = 0; i < block->num; i++) {
	    if (block->info[i].extern_id != TRC_NO_SUCH_EVENT &&
		    strcmp(name, block->info[i].name) == 0) {
		return block->offset + i;
	    }
	}
	block = block->next;
    }
    return TRC_NO_SUCH_EVENT;
}


void
trc_event_lst_query(trc_event_lst_p block, trc_event_t e, size_t *usr_size,
		    int *level)
{
    trc_event_info_p info;

    info = trc_event_lst_locate(block, e);
    *usr_size = info->usr_size;
    *level = info->level;
}


void
trc_event_lst_query_extern(trc_event_lst_p block, trc_event_t e,
			   trc_event_p e_extern, char **name, char **fmt)
{
    trc_event_info_p info;

    info = trc_event_lst_locate(block, e);
    *e_extern = info->extern_id;
    *name = info->name;
    *fmt = info->fmt;
}


size_t
trc_event_lst_usr_size_f(trc_event_lst_p lst, trc_event_t e)
{
    trc_event_info_p info;

    info = trc_event_lst_locate(lst, e);
    return info->usr_size;
}


int
trc_event_lst_level_f(trc_event_lst_p lst, trc_event_t e)
{
    trc_event_info_p info;

    info = trc_event_lst_locate(lst, e);
    return info->level;
}


trc_event_t
trc_event_lst_extern_id_f(trc_event_lst_p lst, trc_event_t e)
{
    trc_event_info_p info;

    info = trc_event_lst_locate(lst, e);
    return info->extern_id;
}


char *
trc_event_lst_name_f(trc_event_lst_p lst, trc_event_t e)
{
    trc_event_info_p info;

    info = trc_event_lst_locate(lst, e);
    return info->name;
}


char *
trc_event_lst_fmt_f(trc_event_lst_p lst, trc_event_t e)
{
    trc_event_info_p info;

    info = trc_event_lst_locate(lst, e);
    return info->fmt;
}


void
trc_event_lst_clear(trc_event_lst_p block)
{
    trc_event_info_p e;
    int              num;
 
    if (block != NULL) {
	trc_event_lst_clear(block->next);
	for (num = 0; num < block->num; num++) {
	    e = &block->info[num];
	    pan_free(e->name);
	    pan_free(e->fmt);
	}
	pan_free(block);
    }
}


int
trc_event_lst_num(trc_event_lst_p block)
{
    int         n = 0;
    int         i;

    while (block != NULL) {
	for (i = 0; i < block->num; i++) {
	    if (block->info[i].level != -1) {
		++n;
	    }
	}
	block = block->next;
    }
    return n;
}


trc_event_t
trc_event_lst_first(trc_event_lst_p block)
{
    return trc_event_lst_next(block, TRC_NO_SUCH_EVENT);
}


trc_event_t
trc_event_lst_next(trc_event_lst_p block, trc_event_t e)
{
    ++e;
    while (e >= block->num) {
	block = block->next;
	if (block == NULL) {
	    return TRC_NO_SUCH_EVENT;
	}
	e -= TRC_EVENT_BLOCK;
    }
    assert(e >= 0 && e < TRC_EVENT_BLOCK && e < block->num);
    while (block != NULL) {
	while (e < block->num) {
	    if (block->info[e].extern_id != TRC_NO_SUCH_EVENT &&
		    block->info[e].level != -1) {
		return block->offset + e;
	    }
	    ++e;
	}
	block = block->next;
	e = 0;
    }
    return TRC_NO_SUCH_EVENT;
}


int
trc_event_tp_set_length(int max_event)
{
    return ((max_event + ULONG_BITS - 1) / ULONG_BITS) *
		sizeof(long unsigned int);
}


trc_event_tp_set_p
trc_event_tp_set_create(int max_event)
{
    return pan_calloc(ULONG_WORD(max_event) + 1, sizeof(long unsigned int));
}


void
trc_event_tp_set_clear(trc_event_tp_set_p set)
{
    pan_free(set);
}


boolean
trc_is_set(trc_event_tp_set_p set, int bit)
{
    return ((set[ULONG_WORD(bit)] & (0x1 << ULONG_BIT(bit))) != 0);
}


void
trc_set(trc_event_tp_set_p set, int bit)
{
    set[ULONG_WORD(bit)] |= 0x1 << ULONG_BIT(bit);
}


void
trc_unset(trc_event_tp_set_p set, int bit)
{
    set[ULONG_WORD(bit)] &= ~(0x1 << ULONG_BIT(bit));
}
