/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_order_list.h"
#include "pan_bg_alloc.h"
#include "pan_bg_global.h"

#include <stdio.h>

/*
 * order_list maintains a list of fragments with sequence numbers that
 * have to wait for an older fragment to arrive. The list is ordered with
 * respect to the sequence numbers (lower numbers first).
 */

typedef struct list{
    struct list    *next;
    seqno_t         seqno;
    pan_fragment_p  frag;
}list_t, *list_p;


/* static variables */
static list_p list;
static list_p tail;
static int    size;

/* pool of list entries */

#define POOL_COUNT 30
static list_p pool;

#define NEW()        st_alloc((void **)&pool, sizeof(list_t), POOL_COUNT)
#define FREE(x)      st_free((x), (void **)&pool, sizeof(list_t))


void
order_start(void)
{
    list = NULL;
    tail = NULL;
    size = 0;
}

void
order_end(void)
{
    assert(list == NULL);
    assert(size == 0);
}

/*
 * order_add:
 *                 Add a fragment to the order list. 
 */

int
order_add(seqno_t seqno, pan_fragment_p fragment)
{
    list_p *head = &list;
    list_p entry;

    /* fill in a new entry */
    entry = NEW();
    entry->seqno = seqno;
    entry->frag  = fragment;
    entry->next  = NULL;

    if (tail && tail->seqno < seqno) {
	tail->next = entry;
	tail = entry;
    }else {
	/* find place in list */
	while(*head != NULL){
	    if ((*head)->seqno == seqno){
		printf("Double entry in order list rejected\n");
		FREE(entry);
		return 0;
	    }

	    if ((*head)->seqno > seqno){
		/* insert entry before head */
		break;
	    }
	    /* next entry */
	    head = &(*head)->next;
	}
	/* insert entry in list */
	entry->next = *head;
	*head = entry;

	if (tail == NULL) tail = list;
    }

    size++;

    return 1;
}

/*
 * order_get:
 *                 Tries to get the fragment with sequence number seqno.
 *                 If it is not in the list, return 0; else remove it and
 *                 all its predecessors from the list, fill in the
 *                 parameters and return 1.
 */

int
order_get(seqno_t seqno, pan_fragment_p *fragment)
{
    list_p tmp;
    int ret = 0;

    while(list && list->seqno <= seqno){
	/* remove all entries that have been ordered */
	tmp = list;
	
	list = list->next;
	if (list == NULL) tail = NULL; /* empty list has no tail */
	
	size--;
	if (tmp->seqno == seqno){
	    /* fill parameters with correct entry */
	    *fragment = tmp->frag;
	    FREE(tmp);
	    ret = 1;
	    break;
	}
	
	fprintf(stderr, "XXX: debug/understand this\n");
	/*
	* I don't understand now when this situation will happen,
	* but I'm quite sure that the fragment has to be cleared
	* somewhere.
	*/
	FREE(tmp);
    }

    /* consistency checks */
    assert(size != 0 || (list == NULL && tail == NULL));
    assert(size != 1 || (list == tail));
    assert(size <= 1 || (list != tail));

    return ret;
}

 
/*
 * order_size:
 *                 Returns the number of unordered pending fragments.
 */

int
order_size(void)
{
    return size;
}
    








