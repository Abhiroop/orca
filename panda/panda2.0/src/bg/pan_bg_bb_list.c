/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_alloc.h"
#include "pan_bg_bb_list.h"
#include "pan_bg_global.h"

/*
 * bb_list manages a list for each platform of all group fragments that
 * arrived without sequence number. The lists are ordered on the local
 * index numbers (lower numbers first).
 *
 * XXX: keep tail pointers
 */

typedef struct list{
    struct list    *next;
    seqno_t         index;
    pan_fragment_p  frag;
}list_t, *list_p;

static list_p *table;

/* pool of list entries */

#define POOL_COUNT 30

static list_p pool;

#define NEW()        st_alloc((void **)&pool, sizeof(list_t), POOL_COUNT)
#define FREE(x)      st_free((x), (void **)&pool, sizeof(list_t))


void
bb_start(void)
{
    int i;

    table = pan_malloc(sizeof(list_p) * pan_nr_platforms());
    assert(table);
    for(i = 0; i < pan_nr_platforms(); i++){
	table[i] = NULL;
    }
}

void
bb_end(void)
{
    int i;

    for(i = 0; i < pan_nr_platforms(); i++){
	assert(table[i] == NULL);
    }
    pan_free(table);
}

/*
 * bb_add:
 *                 Add a fragment to the bb list.
 */
void
bb_add(pan_fragment_p fragment)
{
    pan_bg_hdr_p header = (pan_bg_hdr_p)pan_fragment_header(fragment);
    list_p *head = &table[header->pid];
    list_p entry;

    assert(pan_my_pid() != 0);

    /* fill in a new entry */
    entry = NEW();
    entry->index = header->index;
    entry->frag = fragment;

    /* find place in list */
    while(*head != NULL){
	if ((*head)->index > header->index){
	    /* insert this entry before head */
	    break;
	}
	/* next entry */
	head = &(*head)->next;
    }
    /* insert entry in list */
    entry->next = *head;
    *head = entry;
}

/*
 * bb_find:
 *                 finds the entry with keys (pid, index). If it was in
 *                 the list, remove it, set frag, and return 1. Else
 *                 return 0.
 */

int
bb_find(int pid, seqno_t index, pan_fragment_p *frag)
{
    list_p *head = &table[pid];
    list_p tmp;

    /* search list */
    while(*head != NULL){
	if ((*head)->index == index){
	    /* found the correct entry, fill in parameters */
	    *frag = (*head)->frag;

	    /* remove head from this list */
	    tmp = *head;
	    *head = (*head)->next;
	    FREE(tmp);
	    
	    return 1;
	}else if ((*head)->index > index){
	    /* index not found in sorted list */
	    return 0;
	}
	/* next entry */
	head = &(*head)->next;
    }
    
    /* empty list */
    return 0;
}

/*
   bb_remove
                   Checks if the entry from platform pid with index index
                   are removed from the list. If not, remove them.
*/
	
void
bb_remove(int pid, seqno_t index)
{
    list_p *head = &table[pid];
    list_p tmp;

    /* search list */
    while(*head != NULL){
	if ((*head)->index == index){
	    /* found the correct entry, remove head from this list */
	    tmp = *head;
	    *head = (*head)->next;
	    FREE(tmp);
	    
	    return;
	}else if ((*head)->index > index){
	    /* index not found in sorted list */
	    return;
	}
	/* next entry */
	head = &(*head)->next;
    }
}

