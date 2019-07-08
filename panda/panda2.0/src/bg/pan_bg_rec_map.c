/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Panda distribution.
 */

#include "pan_bg.h"
#include "pan_bg_rec_map.h"
#include "pan_bg_group.h"
#include "pan_bg_send_map.h"
#include "pan_bg_global.h"

/*
 * The receive map contains entries that can be used to assemble ordered
 * fragments. Since all entries arrive in order, all platforms will take
 * the same decision where to assemble the first fragment, so this number
 * can be used to address the other fragments. Each group message
 * contains an index in the send_map, to specifiy where the local sender
 * entry is. If the message is received on the local platform, this entry
 * is signalled, and the receive map entry is passed.
 *
 * When a message is completely received, an upcall is made to the
 * registered upcall_handler.
 *
 * There is a small problem when more messages have to be assembled than
 * there are entries in this table. This can be fixed by using a
 * wait_list (since the fragments are already ordered, the wait_list
 * operations will be consistent), but for the moment we panic on this
 * situation.
 */


typedef struct{
    int          next;
    pan_msg_p    msg;
}entry_t, *entry_p;

static entry_p table;
static int     table_size;
static int     free_list;
static void  (*upcall_handler)(pan_msg_p msg);

void
rec_start(void)
{
    int i;

    table_size = pan_nr_platforms() * 2;
    table = pan_malloc(sizeof(entry_t) * table_size);
    assert(table);

    free_list = -1;
    for(i = table_size - 1; i >= 0; i--){
	table[i].next = free_list;
	table[i].msg  = NULL;
	free_list = i;
    }
}

void
rec_end(void)
{
    pan_free(table);

    free_list = -1;
}

/*
 * find_entry:
 *                 Finds an empty entry in the map.
 */

static int
find_entry(void)
{
    int entry;

    assert(free_list != -1);
    
    entry = free_list;
    free_list = table[entry].next;
    table[entry].msg = pan_msg_create();

    return entry;
}


/*
 * rec_handle:
 *                 Handles a fragment received from the network. The
 *                 fragments must arrive in a total order. If the
 *                 fragment was sent on this platform, the corresponding
 *                 send map entry is signalled.
 */

void
rec_handle(pan_fragment_p fragment, pan_bg_hdr_p header, int flags)
{
    int pid = header->pid;
    int src = header->src;
    pan_msg_p msg;
    int entry;

    if (header->dest == DEST_ANONYMOUS){
	/* first fragment of a message; find empty entry in map */
	entry = find_entry();
    }else{
	/* not the first part, dest is entry */
	entry = header->dest;
    }

    /* assemble fragment to message */
    pan_msg_assemble(table[entry].msg, fragment, 0);
    pan_fragment_clear(fragment);
    header = NULL;

    /* signal the send entry */
    if (pid == pan_my_pid()){
	send_signal(src, entry);
    }

    if (flags & PAN_FRAGMENT_LAST){
	assert(upcall_handler);

	msg = table[entry].msg;

	/*
         * Make the entry free again.
         */
	table[entry].msg  = NULL;
	table[entry].next = free_list;
	free_list = entry;

	/* make an upcall to the handler */
	pan_mutex_unlock(group_lock);
	upcall_handler(msg);
	pan_mutex_lock(group_lock);
    }
}

/*
 * rec_register:
 *                 Registers the upcall handler to which complete group
 *                 messages must be delivered.
 */

void
rec_register(void (*func)(pan_msg_p msg))
{
    assert(upcall_handler == NULL);
    upcall_handler = func;
}
