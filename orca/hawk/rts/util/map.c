#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "map.h"
#include "assert.h"
#include "lock.h"
#include "precondition.h"
#include "misc.h"

#define MODULE_NAME "MAP"

#define num_start_items 5

struct map_s {
  int max_items;
  int num_items;
  boolean *used_index;
  item_p *items;
  po_lock_p map_lock;
};

static  int me;
static  int group_size;
static  int proc_debug;
static  int initialized=0;

static int look_for_empty_entry(item_p *table, boolean *used_index, int num_items);
static int expand_map(map_p map);

/*=================================================================*/
int 
init_map(int moi, int gsize, int pdebug) {
  if (initialized++) return 0;
  me=moi;
  group_size=gsize;
  proc_debug=pdebug;
  init_lock(me,group_size,proc_debug);
  return 0;
}

/*=================================================================*/
int 
finish_map(void) {
  if (--initialized) return 0;
  finish_lock();
  return 0;
}

/*=================================================================*/
/* Creates a new map. */
/*=================================================================*/
map_p 
new_map(void) {
  map_p map;
  int j;

  precondition(initialized);
  map=(map_p)malloc(sizeof(map_t));
  assert(map!=NULL);
  map->items=(item_p *)malloc(num_start_items*sizeof(item_p));
  map->max_items=num_start_items;
  map->num_items=0;
  map->map_lock=new_lock();
  map->used_index=(boolean *)malloc(num_start_items*sizeof(boolean));
  for (j=0; j<num_start_items; j++) map->used_index[j]=FALSE;
  return map;
}


/*=================================================================*/
/* Duplicates a map. */
/*=================================================================*/
map_p 
duplicate_map(map_p map) {
  map_p new;

  precondition_p(map!=NULL);
  new=(map_p)malloc(sizeof(map_t));
  assert(new!=NULL);
  lock(map->map_lock);
  new->items=(item_p *)malloc(map->max_items*sizeof(item_p));
  memcpy(new->items,map->items,sizeof(item_p)*map->max_items);
  new->max_items=map->max_items;
  new->num_items=map->num_items;
  new->map_lock=new_lock();
  new->used_index=(boolean *)malloc(map->max_items*sizeof(boolean));
  memcpy(new->used_index,map->used_index,sizeof(boolean)*map->max_items);
  unlock(map->map_lock);
  return new;
}


/*=================================================================*/
/* Frees a new map. */
/*=================================================================*/
int 
free_map(map_p map) {
  precondition(map!=NULL);
  free(map->items);
  free(map->used_index);
  free_lock(map->map_lock);
  free(map);
  return 0;
}
  
/*=================================================================*/
/* Returns the number of elements in a map. */
/*=================================================================*/
int
num_items(map_p map) {
  precondition(map!=NULL);
  return map->num_items;
}

/*=================================================================*/
/* Returns the number of elements in a map. */
/*=================================================================*/
int
max_items(map_p map) {
  precondition(map!=NULL);
  return map->max_items;
}

/*=================================================================*/
/* Prints the contents of a map. */
/*=================================================================*/
int 
print_map(map_p map) {
  int i, j;

  precondition(map!=NULL);
  printf("max_items: \t%d\n", map->max_items);
  printf("num_items: \t%d\n", map->num_items);
  for (i=0, j=0; i<map->max_items; i++)
    {
      if (((j % 10)==0) && !j) printf("\n");
      if (map->used_index[i])
	{
	  printf("%p\t", map->items[i]);
	  j++;
	}
    }
  printf("\n");
  return 0;
}


/*=================================================================*/
/* Registers an item in a given map. If the map size is too small, it
   doubles it, and then inserts the item. */
/*=================================================================*/

int 
insert_item(map_p map, item_p item) {
  int i;

  precondition(map!=NULL);

  lock(map->map_lock);
  i=look_for_empty_entry(map->items,map->used_index,map->max_items);
  if (i<0) {
    expand_map(map);
    i=look_for_empty_entry(map->items,map->used_index,map->max_items);
    assert((i>=0)&&(i<map->max_items));
  }
  map->used_index[i]=TRUE;
  map->items[i]=item;
  map->num_items++;
  unlock(map->map_lock);
  return i;
}

/*=================================================================*/
/* Registers an item in a given map. If the map size is too small, it
   doubles it, and then inserts the item. */
/*=================================================================*/
int 
insert_index(map_p map, item_p item, int index) {
  precondition(map!=NULL);
  precondition(item_index>=0);
  lock(map->map_lock);
  while (index>=map->max_items)
    expand_map(map);
  map->used_index[index]=TRUE;
  map->items[index]=item;
  map->num_items++;
  unlock(map->map_lock);
  return index;
}

/*=================================================================*/
/* Deletes an item from a map given the item's pointer. */
/*=================================================================*/
int 
remove_item_p(map_p map, item_p item) {
  int i;

  precondition(map!=NULL);
  
  i=item_index(map,item);
  return remove_item(map,i);
}

/*=================================================================*/
/* Deletes an item from a map given the item's index. */
/*=================================================================*/

int 
remove_item(map_p map, int item_index) {
  precondition(map!=NULL);
  precondition((item_index>=0)&&(item_index<map->max_items));

  lock(map->map_lock);
  map->used_index[item_index]=FALSE;
  map->num_items--;
  map->items[item_index]=NULL;
  unlock(map->map_lock);
  return 0;
}

/*=================================================================*/
/* Returns the item corresponding to the item's index. */
/*=================================================================*/
item_p 
get_item(map_p map, int item_index) {
  precondition_p(map!=NULL);

  precondition_p(item_index>=0);

  if (item_index >= map->max_items || !(map->used_index[item_index]))
    return NULL;
  return map->items[item_index];
}

/*=================================================================*/
/* Returns the index of an item in a map. */
/*=================================================================*/
int 
item_index(map_p map, item_p item) {
  int i;

  precondition(map!=NULL);
  
  for (i=0; i<map->max_items; i++)
      if ((map->used_index[i]==TRUE) && (map->items[i]==item))
	break;
  if (i<map->max_items) return i;
  return -1;
}

/*=================================================================*/
/* Frees all items in a map. */
/*=================================================================*/
int 
free_items(map_p map) {
  int i;

  precondition(map!=NULL);
  
  i=0;
  while (remove_item(map,i)>=0)
    i++;
  return 0;
}

/*************************************************************************/
/*********************** Static functions ********************************/
/*************************************************************************/

/*=================================================================*/
/* Looks for an empty entry in a table. */
/*=================================================================*/
int 
look_for_empty_entry(item_p *table, boolean *used_index, int num_items) {
  int i;
  
  assert(table!=NULL);
  assert(used_index!=NULL);
  assert(num_items>=0);

  for (i=0; i<num_items; i++)
     if (!(used_index[i])) return i;
  return -1;
}

/*=================================================================*/
/* Expands the map table by doubling the number of entries. */
/*=================================================================*/
int 
expand_map(map_p map) {
  boolean *new_used_index;
  item_p new_items;
  int i;
  
  assert(map!=NULL);

  new_items=(item_p *)malloc(map->max_items*2*sizeof(item_p));
  assert(new_items!=NULL);
  new_used_index=(boolean *)malloc(map->max_items*2*sizeof(boolean));
  assert(new_used_index!=NULL);
  memcpy(new_items,map->items,map->max_items*sizeof(item_p));
  memcpy(new_used_index,map->used_index,map->max_items*sizeof(boolean));
  for (i=map->max_items; i<map->max_items*2; i++)
    new_used_index[i]=FALSE;
  free(map->used_index);
  free(map->items);
  map->used_index=new_used_index;
  map->items=new_items;
  map->max_items=map->max_items*2;
  return 0;
}
