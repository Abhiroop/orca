#ifdef PARIX_T800


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/root.h>
#include <sys/link.h>
#include <sys/time.h>

#include "pan_system.h"
#include "pan_error.h"
#include "pan_malloc.h"

#include "pan_parix.h"


#define PRESERVE_LINKS    12	/* # virtual links to keep on non-Sequecer
				 * nodes */


static int            g_vlinks = 0;
static RootContext_t *rtxt = NULL;
static Context_t     *ctxt = NULL;
static Thread_t      *thr = NULL;
static Thread_t      *prev_thread;
static int            search_overhead1 = -1;
static int            search_overhead2 = 0;
static void          *begin_block = NULL;
static void          *after_block = NULL;


static void
free_thread_struct(byte * stack, VLCB_t * this_vlcb, int this_index)
{				/* find and free thread and stack */
    Thread_t   *next, *prev;
    int         size;
    int        *ip;

    if (rtxt == NULL) {		/* in first call: initialize context pointers */
	rtxt = GET_ROOT()->ContextRoot;
	assert(rtxt->nContext > 1);

	ctxt = (Context_t *)rtxt->ContextRoot.Head;
	assert(ctxt->id == 1);
	assert(ctxt->nThread > g_vlinks * 2);
    }
    if (thr == NULL) {		/* first call or at end of list: re-set
				 * pointers */
	thr = (Thread_t *)ctxt->threads.Head;
	prev_thread = (Thread_t *)thr->link.Prev;
	assert(thr != NULL);

	while (!(thr->MDL.StackLo <= stack && thr->MDL.StackHi >= stack)
		&& thr != NULL) {	/* search */
	    prev_thread = thr;
	    thr = (Thread_t *)thr->link.Next;
	}
	assert(thr != NULL);

	search_overhead1++;
    }
    while (!(thr->MDL.StackLo <= stack && thr->MDL.StackHi >= stack)
	    && thr != NULL) {
	prev_thread = thr;
	thr = (Thread_t *)thr->link.Next;	/* search for thread */
	search_overhead2++;
    }
    assert(thr != NULL);	/* it has to be there! */

    size = (int)thr->MDL.StackHi - (int)thr->MDL.StackLo;
    assert(size >= 86 && size <= 94);	/* it has to have correct size! */

    ip = (int *)thr->MDL.StackLo;	/* test if pointers match */
    assert(((VLCB_t *)ip[1]) == this_vlcb);

    ip = (int *)thr->MDL.StackHi;	/* test if indexes match */
    assert(((int)ip[-1]) == this_index);


    if (ip + 1 != (int *)prev_thread) {
	pan_sys_printf("failure: %x %x %x\n", ip, ip + 1, prev_thread);
	assert(ip + 1 == (int *)prev_thread);	/* test block layout */
    }
    size = mallsize(thr);	/* test block layout */
    assert(size == sizeof(Thread_t));
    assert(((byte *)thr) + size + sizeof(size_t) == thr->MDL.StackLo);

    next = (Thread_t *)thr->link.Next;	/* remove Thread_t from list */
    prev = (Thread_t *)thr->link.Prev;

    assert(next != NULL);
    assert(prev != NULL);
    if (ctxt->threads.Tail != (Node_t *)thr)
	next->link.Prev = (Node_t *)prev;
    else
	ctxt->threads.Tail = (Node_t *)prev;
    if (ctxt->threads.Head != (Node_t *)thr)
	prev->link.Next = (Node_t *)next;
    else
	ctxt->threads.Head = (Node_t *)next;
    ctxt->nThread--;

    if (after_block == NULL)	/* remember borders of area to free */
	after_block = (void *)ip;	/* thr->MDL.StackHi */
    begin_block = (void *)((byte *)thr - sizeof(size_t));

    prev_thread = thr;		/* go to next Thread_t */
    thr = next;
}

#define THIS_USED  1
#define PREV_USED  2

static int
my_free(void *begin_ptr, void *after_ptr)
{
    size_t     *p, *next, size;

    next = (size_t *)after_ptr;
    p = (size_t *)begin_ptr;
    size = (byte *)next - (byte *)p - sizeof(size_t);

    next[-1] = THIS_USED;
    p[0] = size | THIS_USED | PREV_USED;

    p++;
    size -= sizeof(size_t);
    if (mallsize(p) != size) {
	pan_sys_printf("FAILURE: %d %d\n", mallsize(p), size);
	return 0;
    }
    free(p);
    return 1;
}

static void
free_links(int *to_free, int start, int end, int hold)
{
    int         i, index;
    VLink_t    *vlink, *first_vlink, *prev_vlink;
    LinkCB_t   *linkcb, *first_linkcb, *prev_linkcb;
    void       *after_linkcb;
    VLCB_t     *vlcb, *first_vlcb, *prev_vlcb;
    RootLink_t *my_rl;

    assert(hold >= 0);
    my_rl = GET_ROOT()->LinkRoot;

    index = to_free[start];
    first_vlink = prev_vlink = vlink = &(my_rl->VLinks[index]);
    first_linkcb = prev_linkcb = linkcb = &(my_rl->StartVLinks[index]);
    first_vlcb = prev_vlcb = vlcb = &(my_rl->VLCBs[index]);

/*
   my_rl->CountOfVLinks--;
*/

    free_thread_struct((byte *)vlink->in, vlcb, index);
    free_thread_struct((byte *)vlink->out, vlcb, index);

    for (i = start + 1; i < end; i++) {
	index = to_free[i];
	vlink = &(my_rl->VLinks[index]);
	linkcb = &(my_rl->StartVLinks[index]);
	vlcb = &(my_rl->VLCBs[index]);

	assert((byte *)vlink == (byte *)prev_vlink + sizeof(VLink_t));
	assert((byte *)linkcb == (byte *)prev_linkcb + sizeof(LinkCB_t));
	assert((byte *)vlcb == (byte *)prev_vlcb + sizeof(VLCB_t));

/*
    my_rl->CountOfVLinks--;
*/

	free_thread_struct((byte *)vlink->in, vlcb, index);
	free_thread_struct((byte *)vlink->out, vlcb, index);

	prev_vlink = vlink;
	prev_linkcb = linkcb;
	prev_vlcb = vlcb;
    }

/*
   assert( hold == my_rl->CountOfVLinks );
*/

    if (realloc(my_rl->VLCBs, hold * sizeof(VLCB_t)) != my_rl->VLCBs)
	pan_sys_printf("realloc of VLCBs failed\n");

    if (realloc(my_rl->VLinks, hold * sizeof(VLink_t)) != my_rl->VLinks)
	pan_sys_printf("realloc of VLinks failed\n");

/*
   assert( mallsize( my_rl->LinkCBs ) == sizeof( LinkCB_t ) *
	      ( my_rl->CountOfVLinks + (end - start) +
		 2*my_rl->CountOfLLinks + 2*my_rl->CountOfRLinks ) );
*/
    assert(mallsize(my_rl->LinkCBs) == sizeof(LinkCB_t) *
	   (my_rl->CountOfVLinks +
	    2 * my_rl->CountOfLLinks + 2 * my_rl->CountOfRLinks));

    after_linkcb = (void *)(((byte *)first_linkcb) +
			(end - start) * sizeof(LinkCB_t));

/*
    printf( "search_overhead1: %d  search_overhead2: %d\n",
		search_overhead1, search_overhead2 );
   printf( "workspace: BEGIN %8x   AFTER %8x\n", begin_block, after_block );
    printf( "structure: BEGIN %8x   AFTER %8x\n", first_linkcb, after_linkcb );
*/

    if (!my_free(begin_block, after_block))
	pan_sys_printf("myfree ( workspace )failed\n");

/*
   if ( !my_free( first_linkcb, after_linkcb ) )
	pan_sys_printf( "myfree ( structure )failed\n" );
*/
}

void
exhaust_vlinks(void)
{
    int        *got_links;
    int         new_link, preserve_count, free_count, index_count, kernel_links,
                i;
    RootLink_t *my_rl;

    my_rl = GET_ROOT()->LinkRoot;
    g_vlinks = my_rl->CountOfVLinks;
    got_links = malloc(my_rl->CountOfVLinks * sizeof(int));

    index_count = preserve_count = free_count = 0;
    while ((new_link = AllocVLink()) >= 0 && preserve_count < PRESERVE_LINKS) {
	got_links[index_count] = new_link;
	preserve_count++;
	index_count++;
    }
    if (new_link >= 0 && index_count < my_rl->CountOfVLinks) {
	do {
	    got_links[index_count] = new_link;
	    assert(got_links[index_count] == got_links[index_count - 1] + 1);
	    free_count++;
	    index_count++;
	} while ((new_link = AllocVLink()) >= 0 &&
		    index_count < my_rl->CountOfVLinks);
    }

    kernel_links = my_rl->CountOfVLinks - preserve_count - free_count;
    assert(kernel_links >= 0);

/*
    printf( "%d> VLINKS: preserve: %d   free: %d   total: %d / kernel: %d\n",
	   GET_ROOT()->ProcRoot->MyProcID, preserve_count,
	      free_count, index_count, kernel_links );
*/

    if (index_count > preserve_count)
	free_links(got_links, preserve_count, index_count,
		   preserve_count + kernel_links);

    for (i = 0; i < preserve_count; i++)
	if (FreeVLink(got_links[i]) != 0)
	    pan_sys_printf("free of VLink %d failed\n", got_links[i]);

    free(got_links);
}


#endif		/* PARIX_T800 */
