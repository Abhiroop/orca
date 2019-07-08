/*#####################################################################*/
/* Saniya Ben Hassen. */
/* Interface of a set ADT. */
/*#####################################################################*/

#ifndef __set__
#define __set__

#ifdef PRECONDITION_ON
#define member(s,m) is_member(s,m)
#else
#define member(s,m) ((s)->list_members[m])
#endif

#include "misc.h"

typedef struct set_s *set_p, set_t;

struct set_s { 
  int max_members;
  int num_members;
  boolean *list_members; 
};

set_p new_set(int max_members);
int free_set(set_p set);
set_p duplicate_set(set_p set);

int set_union(set_p set1, set_p set2);

int add_member(set_p set, int member);
int remove_member(set_p set, int member);
int add_list_of_members(set_p set, int *list, int listl);
int empty_set(set_p set);
int full_set(set_p set);

boolean equal_sets(set_p s1, set_p s2);
boolean is_member(set_p set, int member);
boolean is_empty(set_p set);
int *list_of_members(set_p set);

int print_set(set_p set);

set_p unmarshall_set(void *mset);
void *marshall_set(set_p set, int *mset_size);

#endif

