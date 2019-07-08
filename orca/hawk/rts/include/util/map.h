#ifndef __map__
#define __map__

#include "misc.h"

typedef void *item_p;
typedef struct map_s *map_p, map_t;

int init_map(int me, int group_size, int proc_debug);
int finish_map(void);

map_p new_map(void);
int free_map(map_p map);
map_p duplicate_map(map_p map);

int print_map(map_p map);
int num_items(map_p map);
int max_items(map_p map);
int insert_item(map_p map, item_p item);
int insert_index(map_p map, item_p item, int index);
int remove_item_p(map_p map, item_p item);
int remove_item(map_p map, int item_index);
item_p get_item(map_p map, int index);
int item_index(map_p map, item_p item);
int free_items(map_p map);

#endif

