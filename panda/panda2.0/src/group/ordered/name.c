#include <string.h>
#include <assert.h>

#include "pan_sys.h"

#include "name.h"


/* Name:
 *   Maps group ids to group names.
 *
 * Notes:
 * o Table is implemented as a monitor.
 * o Index in table is group id.
 */


typedef struct map_entry map_entry_t, *map_entry_p;

struct map_entry {
    char       *name;
};


static map_entry_p bindings;
static int         tabsize;
static int         increment;
static int         nr_bindings;
static int         free_slot = -1;

static pan_mutex_p table_lock;


void
pan_name_start(int init_size, int extend_size)
{
    int         i;

    increment = extend_size;
    tabsize = init_size;
    bindings = pan_malloc(init_size * sizeof(map_entry_t));
    for (i = 0; i < init_size; i++) {
	bindings[i].name = NULL;
    }

    table_lock = pan_mutex_create();
}


void
pan_name_end(void)
{
    int         i;

    /* added following 6 lines */
    for (i = 0; i < tabsize; i++) {
	if (bindings[i].name) {
	    pan_free(bindings[i].name);
	    nr_bindings--;
	}
    }

    pan_free(bindings);
    pan_mutex_clear(table_lock);
}


group_id_t
pan_name_enter(char *name)
{
    int         i, newid;
    int         namelen = strlen(name) + 1;
    char       *tabname;

    pan_mutex_lock(table_lock);

    /* Search for existing binding */
    for (i = 0; i < tabsize; i++) {
	tabname = bindings[i].name;
	if (tabname && strcmp(tabname, name) == 0) {	/* found one */
	    pan_mutex_unlock(table_lock);
	    return (group_id_t)i;
	}
    }


    /* Extend table when it is full */
    if (nr_bindings == tabsize) {
	tabsize += increment;
#ifdef VERBOSE
	Printf("realloc bindings %lx tabsize %d sizeof map_entry_t %d\n",
	       bindings, tabsize, sizeof(map_entry_t));
#endif
	bindings = pan_realloc(bindings, tabsize * sizeof(map_entry_t));
	for (i = nr_bindings; i < tabsize; i++) {
	    bindings[i].name = NULL;
	}
    }
    /* Search for an empty slot */
    do {
	free_slot = (free_slot + 1) % tabsize;
    } while (bindings[free_slot].name);


    /* Enter new entry */
    newid = free_slot;
    assert(namelen != 0);
    bindings[newid].name = pan_malloc(namelen);
    (void)strcpy(bindings[newid].name, name);

    nr_bindings++;

    pan_mutex_unlock(table_lock);

    return (group_id_t)newid;
}


void
pan_name_delete(group_id_t gid)
{
    pan_mutex_lock(table_lock);
    assert(bindings[(int)gid].name);

    pan_free(bindings[gid].name);
    bindings[gid].name = NULL;	/* added by HPH */
    nr_bindings--;

    pan_mutex_unlock(table_lock);
}
