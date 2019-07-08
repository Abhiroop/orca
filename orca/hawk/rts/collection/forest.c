#include <stdlib.h>
#include <string.h>

#include "forest.h"
#include "set.h"
#include "assert.h"

static int group_size;
static int initialized=0;
static int me;
static int proc_debug;

static forest_p all_roots(set_p members);
static forest_p binomial_tree_n(int me, int group_size);
static forest_p binomial_forest_n(int moi, int gsize, int cluster_length);
static forest_p binomial_forest_list(int moi, set_p members, int cluster_length);

static int shift_forest(forest_p forest, int s);
static int who_is_child(int i,int st,int n);
static int who_is_parent(int i,int st,int n);
static int depth(int i, int group_size);
static set_p is_root(int num_roots, int *roots);
static set_p is_child(int num_children, int *children);
static int check_forest(forest_p forest);
static int treesize = 0;


/*========================================================================*/
/*========================================================================*/
/*========================= Exported Functions ===========================*/
/*========================================================================*/
/*========================================================================*/

int 
init_forest(int moi, int gsize, int pdebug, int *argc, char **argv) {
  int i,j;
  if (initialized++) return 0;
  group_size=gsize;
  me=moi;
  proc_debug=pdebug;
  j = 1;
  for(i = 1; i < *argc; i++){
	if (strcmp(argv[i], "-hawk_treesize") == 0) {
	    i++;
	    treesize = atoi(argv[i]);
	} else {
	    argv[j++] = argv[i];
	}
  }
  *argc = j;
  return 0;
}
       
/*========================================================================*/
int 
finish_forest(void) {
  if (--initialized) return 0;
  return 0;
}
  
/*========================================================================*/
/* Creates a new forest. */
forest_p 
new_forest(set_p members) {
  /* The numbers used in this function are usually the best for a gather-all,
     on 10Mbit/sec Ethernet. For most applications it is probably best to
     try several tree sizes.
  */
  forest_p forest;

  if (! is_member(members, me)) return 0;
  if (treesize) {
    forest=binomial_forest_list(me,members,treesize);
  }
  else if (members->num_members<=4) {
    /* In this case, two roots is better because of the multicast -> unicast
       trick.
    */
    forest=all_roots(members);
  }
  else if (members->num_members <= 8) {
    forest=binomial_forest_list(me,members,2);
  }
  else if (members->num_members <= 16) {
    forest=binomial_forest_list(me,members,4);
  }
  else {
    forest=binomial_forest_list(me,members,8);
  }
  assert(check_forest(forest)>=0);
  /* print_forest(forest); */
  return forest;
}


/*========================================================================*/
int 
free_forest(forest_p forest) {

  if (! forest) return 0;
  if (forest->children!=NULL) free(forest->children);
  if (forest->roots!=NULL) free(forest->roots);
  if (forest->isroot!=NULL) free_set(forest->isroot);
  if (forest->ischild!=NULL) free_set(forest->ischild);
  free(forest);
  return 0;
}

/*========================================================================*/
/* Prints a forest in a human readable form. */

int 
print_forest(forest_p forest) {
  int i;
  
  printf("*******\n");
  printf("parent: \t%d\n", forest->parent);
  printf("node: \t\t%d\n",me);
  printf("children:");
  for (i=0; (i<forest->fanout); i++)
    printf("\t%d ", forest->children[i]);
  printf("\nroots:  ");
  for (i=0; i<forest->num_roots; i++)
    printf("\t%d", forest->roots[i]);
  printf("\nisroot: \n");
  print_set(forest->isroot);
  printf("\nischild: \n");
  print_set(forest->ischild);
  printf("\n");
  printf("\ntime out weight = %5.3f\n",forest->weight);
  return 0;
}

/*========================================================================*/
/*========================================================================*/
/*========================== Static Functions ============================*/
/*========================================================================*/
/*========================================================================*/

/*========================================================================*/
/*===================== Forest bulding Functions =========================*/
/*========================================================================*/

/*========================================================================*/
/* Given a process number and a group size, this function builds an
   binomial spanning tree. Typically, the rightmost nodes in every
   part of the tree have more children than others. This is a
   desirable property if communication overhead due to message
   handling is very large. */
/*========================================================================*/

forest_p  
binomial_tree_n(int me, int group_size) {
  int p;
  int phase, power;
  int *children;
  forest_p forest;

  forest=(forest_p )malloc(sizeof(forest_t));
  forest->fanout=0;
  children=(int *)malloc(group_size*sizeof(int));
  for (phase=0,power=1;power<group_size;phase++,power=power<<1) { 
    children[phase]=who_is_child(me,phase,group_size);
    if (children[phase]!=(-1))  (forest->fanout)++;
    p=who_is_parent(me,phase,group_size);
    if (p>=0) forest->parent=p;
  }
  if (me==0) forest->parent=-1;
  (forest->children)=(int *)malloc((forest->fanout)*sizeof(int));
  memcpy((char *)(forest->children),(char *)children,(forest->fanout)*sizeof(int));
  forest->roots=(int *)malloc(sizeof(int));
  *forest->roots=0;
  forest->num_roots=1;
  forest->isroot=is_root(forest->num_roots,forest->roots);
  forest->ischild=is_child(forest->fanout,forest->children);
  free(children);
  forest->weight=(float)depth(me,group_size);
  return forest;
}

/*========================================================================*/
/* Creates a spanning structure where all members are roots. */
/*========================================================================*/

forest_p
all_roots(set_p members) { 
  forest_p forest;
  int i,j;

  forest = (forest_p )malloc(sizeof(forest_t));
  forest->children=NULL;
  forest->fanout=0;
  forest->parent=-1;
  forest->roots=(int *)malloc(sizeof(int)*members->num_members);
  for (i=0,j=0; i<group_size;i++)
    if (member(members,i)) {
      forest->roots[j]=i;
      j++;
    }
  forest->num_roots=members->num_members;
  forest->isroot=is_root(forest->num_roots,forest->roots);
  forest->ischild=is_child(forest->fanout,forest->children);
  forest->weight=(float)(me)/members->num_members;
  return forest;
}

/*========================================================================*/
/* This procedure returns a forest in which each tree is a binomial
   tree whith at most cluster_length nodes. */
/*========================================================================*/
forest_p
binomial_forest_n(int moi, int gsize, int cluster_length) {
  int i,j;
  forest_p forest;

  /* Build the binomial tree 'moi' belongs to */
  for (i=0; i<gsize; i += cluster_length) 
    if ((moi>=i) && (moi<i+cluster_length)) {
      forest = binomial_tree_n(moi-i, min(cluster_length,gsize-i));
      shift_forest(forest,i);
      break;
    }
  /* Compute roots */
  free(forest->roots);
  free_set(forest->isroot);
  free_set(forest->ischild);
  forest->num_roots=ceiling(gsize,cluster_length);
  (forest->roots)=(int *)malloc((forest->num_roots)*sizeof(int));
  for (i=0, j=0; i<gsize; i=i+cluster_length, j++) (forest->roots)[j]=i;
  forest->isroot=is_root(forest->num_roots,forest->roots);
  forest->ischild=is_child(forest->fanout,forest->children);
  return forest;
}

/*========================================================================*/
/* This procedure returns a forest in which each tree is a binomial
   tree whith at most cluster_length nodes. */
/*========================================================================*/
forest_p
binomial_forest_list(int moi, set_p members, int cluster_length) {
  int *list,i;
  forest_p forest;

  list=list_of_members(members);
  /* find index of me */
  for (i=0; i<members->num_members; i++)
    if (list[i]==moi) break;
  if (i == members->num_members) i = -1;
  forest=binomial_forest_n(i,members->num_members,cluster_length);
  /* renumber nodes of the forest */
  for (i=0; i<forest->fanout; i++)
    forest->children[i] = list[forest->children[i]];
  if (forest->parent >= 0) forest->parent = list[forest->parent];
  for (i=0; i<forest->num_roots; i++)
    forest->roots[i] = list[forest->roots[i]];
  free_set(forest->isroot);
  free_set(forest->ischild);
  forest->isroot=is_root(forest->num_roots,forest->roots);
  forest->ischild=is_child(forest->fanout,forest->children);
  forest->weight=(float)depth(moi,members->num_members);
  free(list);
  return forest;
}

/*========================================================================*/
/*=========================== Misc. Functions ============================*/
/*========================================================================*/

int
shift_forest(forest_p forest, int s) {
  int i;

  for (i=0; i<forest->fanout; i++)
    forest->children[i] += s;
  if (forest->parent >= 0) forest->parent += s;
  for (i=0; i<forest->num_roots; i++)
    forest->roots[i] += s;
  free_set(forest->isroot);
  free_set(forest->ischild);
  forest->isroot=is_root(forest->num_roots,forest->roots);
  forest->ischild=is_child(forest->fanout,forest->children);
  forest->weight=(float)depth(me,group_size);
  return 0;
}

/*========================================================================*/
/* Determines who is the st-th child of i for a binomial tree with n
   members. */

int 
who_is_child(int i,int st,int n) {
    int res;
    int p1,p0;
    
    p1=(int )my_pow(2,st+1);
    p0=(int )my_pow(2,st);
    if ((i % p0) != 0) return -1;
    if ((i % p1) < p0) 
	res= (i + p0);
    else 
	res= (i - p0);
    if (res>=n) return -1;
    if (res < i) return -1;
    return res;
}

/*========================================================================*/
/* Determines who is the parent of i for a binomial tree with n
   members. */

int 
who_is_parent(int i,int st,int n) {
    int res;
    int p1,p0;
    
    p1=(int )my_pow(2,st+1);
    p0=(int )my_pow(2,st);
    if ((i % p1) < p0) 
	res= (i + p0);
    else 
	res= (i - p0);
    if (res >= n) return -1;
    if (res > i) return -1;
    if (((res % p0) !=0) && (res!=0))  return -1;
    return res;
}


/*========================================================================*/
/* Computes the depth of a node on a binomial tree with group_size
   members. */

int 
depth(int i, int group_size) {
  int j;
  if (i==0)
    return log2(group_size)+1;
  j=1;
  while (!(i & 1)) { 
    j++;
    i= i >> 1;
  }
  return j;
}

/*========================================================================*/
/* Creates and returns a bit_map of the roots given the list of roots. */

set_p 
is_root(int num_roots, int *roots) {
  set_p isroot;
  
  isroot=new_set(group_size);
  add_list_of_members(isroot,roots,num_roots);
  return isroot;
}


/*========================================================================*/
/* Creates and returns a bit_map of the children given the list of children. */

set_p 
is_child(int num_children, int *children) {
  set_p ischild;
  
  ischild=new_set(group_size);
  if (num_children==0)
    empty_set(ischild);
  else
    add_list_of_members(ischild,children,num_children);
  return ischild;
}


/*========================================================================*/
/* This procedure checks the consistency of a spanning forest. It is
   useful for debugging when a new spanning tree is provided in this
   module. */

int 
check_forest(forest_p forest) {
  int i;

  for (i=0; i<forest->fanout; i++)
    if ((forest->children[i]<0)&&(forest->children[i]>=group_size)) {
      fprintf(stderr,"incorrect children\n");
      return -1;
    }
  for (i=0; i<forest->num_roots; i++)
    if ((forest->roots[i]<0)&&(forest->roots[i]>=group_size)) {
      fprintf(stderr,"incorrect roots\n");
      return -1;
    }
  return 0;
}


