/*=====================================================================*/
/*==== nodelist.h : nodelist data structure and function prototypes====*/
/*==                                                                 ==*/

#ifndef node_list_h
#define node_list_h

#include <stdio.h>
#include "node.h"

/*---------------------------------------------------------------------*/
/*---- types                                                           */
/*---------------------------------------------------------------------*/

typedef struct node_elt {
    Node_t          elt;		/* data */
    struct node_elt *next;		/* pointer to next element */
} Node_list_elt_t;

typedef Node_list_elt_t *Node_list_elt_p;

typedef struct{
    int             nr_of_elts;		/* nr of elements in the list */
    Node_list_elt_p head, tail;		/* pointers to head and tail of list */
} Node_list_t;

typedef Node_list_t *Node_list_p;


/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PUB Boolean create_node_list(             /* Out : enough memory? */
int  	        nr_of_elts,               /* In  : nr of elements */
Node_list_p	nl);                      /* Out : the node list */


PUB Boolean add_incoming(                 /* Out : enough memory */
int          	node_nr,                  /* In  : node nr */
Edge_p          edge,                     /* In  : the new incoming edge */
Node_list_p  	nl                        /* Out : updated node list */
);                                        /* Out : address of added edge */ 

PUB Boolean add_outgoing(                 /* Out : enough memory */
int       	node_nr,                  /* In  : node nr */
Edge_p       	edge,                     /* In  : the new outgoing edge */
Node_list_p  	nl                        /* Out : updated node list */
); 


PUB void delete_node_list(
Node_list_p     nl);                      /* Out : the job to be deleted */


PUB Boolean get_node_info(                /* Out : correct index       */
Node_list_t     nl,	  	          /* In  : node list             */
int             index,                    /* In  : the index in the list */
Node_p          node);                    /* Out : the node              */


PUB int nr_of_nodes(                      /* Out: nr of elements         */
Node_list_t     nl);                      /* In : the node_list          */

#endif /* nodelist_h */
