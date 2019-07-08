/*#####################################################################*/
/* Author: Saniya Ben Hassen.
   Implementation of a set ADT (no replicas allowed).
   - Creation and deletion
   - duplication
   - union
   - add and remove member(s)
   - initialization to full or empty set
   - equality test
   - membership test
   - emptyness test
   - marshalling and unmarshalling
   - printing. */
/*#####################################################################*/

#include <stdlib.h>
#include <string.h>
#include "assert.h"
#include "set.h"
#include "precondition.h"

/*========================================================================*/
/* Creates a new empty set. */
/*========================================================================*/

set_p 
new_set(int max_members) {
  set_p set;
  int i;

  precondition_p(max_members>0);

  set=(set_p )malloc(sizeof(set_t));
  assert(set!=NULL);
  set->num_members=0;
  set->max_members=max_members;
  set->list_members=(boolean *)malloc(sizeof(boolean)*max_members);
  for (i=0; i<max_members; i++) set->list_members[i]=FALSE;
  return set;
}

/*========================================================================*/
/* Creates a new set and copies in it the value of the given set. */
/*========================================================================*/

set_p 
duplicate_set(set_p set) {
  set_p set_copy;

  precondition_p(set!=NULL);

  set_copy=(set_p )malloc(sizeof(set_t));
  assert(set_copy!=NULL);
  set_copy->num_members=set->num_members;
  set_copy->max_members=set->max_members;
  set_copy->list_members=(boolean *)malloc(sizeof(boolean)*set->max_members);
  memcpy(set_copy->list_members,set->list_members,
	 sizeof(boolean)*set->max_members);
  return set_copy;
}

/*========================================================================*/
/* Frees a member set. */
/*========================================================================*/

int 
free_set(set_p set) {
  precondition(set!=NULL);
  
  free(set->list_members);
  free(set);
  return 0;
}



/*========================================================================*/
/* Check whether a member is part of a set. */
/*========================================================================*/

boolean 
is_member(set_p set, int member) {
  precondition(member>=0);
  
  if (set==NULL) return FALSE;
  if (member>=set->max_members) return FALSE;
  return set->list_members[member];
}

/*========================================================================*/
/* Adds a member to a set. */
/*========================================================================*/

int 
add_member(set_p set, int member) {
  precondition(set!=NULL);
  precondition((member>=0)&&(member<set->max_members));

  if (is_member(set,member)) return 0;
  set->num_members++;
  assert(set->num_members <= set->max_members);
  set->list_members[member]=TRUE;
  return 0;
}

/*========================================================================*/
/* Removes a member from a set. Does not check whether the member is
   already in the set. */
/*========================================================================*/

int 
remove_member(set_p set, int member) {
  precondition(set!=NULL);
  precondition((member>=0)&&(member<set->max_members));

  if (!(is_member(set,member))) return 0;
  set->num_members--;
  assert(set->num_members >= 0);
  set->list_members[member]=FALSE;
  return 0;
}

/*========================================================================*/
/* Adds a list of members in a set. */
/*========================================================================*/

int 
add_list_of_members(set_p set, int *list, int listl) {
  int i;
  
  precondition(set!=NULL);

  for (i=0; i<listl; i++) add_member(set,list[i]);
  return 0;
}

/*========================================================================*/
/* Computes the union of set1 and set2 in set1. */
/*========================================================================*/
int 
set_union(set_p set1, set_p set2) {
  int i;

  precondition(set1->max_members>=set2->max_members);
  
  for (i=0; i<set2->max_members; i++)
    if (is_member(set2,i)) add_member(set1,i);
  return 0;
}


/*========================================================================*/
/* Empties a set. */
/*========================================================================*/

int 
empty_set(set_p set) {
  int i;

  precondition(set!=NULL);

  set->num_members=0;
  for (i=0; i<set->max_members; i++) set->list_members[i]=FALSE;
  return 0;
}


/*========================================================================*/
/* Fills a set with all members. */
/*========================================================================*/

int 
full_set(set_p set) {
  int i;

  precondition(set!=NULL);
  
  for (i=0; i<set->max_members; i++) add_member(set,i);
  return 0;
}


/*========================================================================*/
/* Prints the contents of a set on stdout. */
/*========================================================================*/

int 
print_set(set_p set) {
  int i;
  int num_printed=0;

  precondition(set!=NULL);

  for (i=0; i<set->max_members; i++)
    if (is_member(set,i)) { 
      printf("%d \t", i);
      fflush(stdout);
      num_printed++;
      if ((num_printed!=0)&&((num_printed % 10)==0)) 
	printf("\n              \t");
    }
  printf("\n");
  return 0;
}

/*========================================================================*/
/* Tests whether a set is empty. */
/*========================================================================*/

boolean 
is_empty(set_p set)
{
  if (set==NULL) return TRUE;
  return (set->num_members==0);
}

/*========================================================================*/
/*========================================================================*/
boolean 
equal_sets(set_p s1, set_p s2) {
  int i;

  precondition(s1!=NULL);
  precondition(s2!=NULL);

  if ((s1->max_members==s2->max_members) &&
      (memcmp(s1->list_members, s2->list_members, s1->max_members)==0))
    return TRUE;
  
  if ((s1->max_members>s2->max_members) &&
      (memcmp(s1->list_members, s2->list_members,s2->max_members)==0)) { 
    for (i=s2->max_members; i<s1->max_members; i++)
      if (s1->list_members[i]) return FALSE;
    return TRUE;
  }

  if ((s2->max_members>s1->max_members) &&
      (memcmp(s1->list_members, s2->list_members,s1->max_members)==0)) { 
    for (i=s1->max_members; i<s2->max_members; i++)
      if (s2->list_members[i]) return FALSE;
    return TRUE;
  }
  return FALSE;
}

/*========================================================================*/
/* Creates and returns the list of members. */
/*========================================================================*/

int *
list_of_members(set_p set) {
  int *list;
  int i,j;

  precondition_p(set!=NULL);

  if (set->num_members==0) return NULL;

  list=(int *)malloc(set->num_members*sizeof(int));
  assert(list!=NULL);
  j=0;
  for (i=0; i<set->max_members; i++)
    if (is_member(set,i)) { 
      list[j]=i;
      j++;
    }
  assert(j==set->num_members);
  return list;
}


/*========================================================================*/
/* Marshalls a set into a newly created buffer. */
/*========================================================================*/
void *
marshall_set(set_p set, int *mset_size) {
  char *mset;
  int sz = 3*sizeof(int)+set->max_members*sizeof(boolean);
  
  precondition_p(set!=NULL);
  precondition(mset_size!=NULL);

  mset=malloc(sz);
  assert(mset!=NULL);
  mset += sizeof(int);
  memcpy(mset,set,2*sizeof(int));
  memcpy(mset+2*sizeof(int),set->list_members,set->max_members*sizeof(boolean));
  (*mset_size)=2*sizeof(int)+set->max_members*sizeof(boolean);
  mset -= sizeof(int);
  *(int *)mset = sz;
  return mset;
}

/*========================================================================*/
/* Unmarshalls a set into a newly created set. */
/*========================================================================*/
set_p 
unmarshall_set(void *mset) {
  set_p set;
  char *p = mset;

  precondition_p(mset!=NULL);

  p += sizeof(int);	/* skip size of marshalled buffer */
  set=new_set(*(int *)p);
  memcpy(set,p,2*sizeof(int));
  memcpy(set->list_members,p+2*sizeof(int),set->max_members*sizeof(boolean));
  return set;
}
