/*=====================================================================*/
/*==== edgelist.h : edgelist data structure function prototypes    ====*/
/*==                                                                 ==*/

#ifndef edgelist_h
#define edgelist_h

#include <stdio.h>
#include <strings.h>
#include "types.h"
#include "edge.h"

/*---------------------------------------------------------------------*/
/*---- types                                                           */
/*---------------------------------------------------------------------*/

typedef struct edge_list_elt{
    Edge_p               elt;
    struct edge_list_elt *next;
} Edge_list_elt_t;

typedef Edge_list_elt_t *Edge_list_elt_p;

typedef struct {
    int             nr_of_elts;
    Edge_list_elt_p head, tail;
} Edge_list_t;

typedef Edge_list_t *Edge_list_p;


/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/


PUB Boolean create_edge_list(           /* Out : enough memory?        */
Edge_list_p  	el);                    /* Out : the edge list         */


PUB Boolean add_edge(                   /* Out : enough memeory?       */
Edge_list_p  el,                        /* Out : the jobqueue          */
Edge_p       elt                        /* In  : The element to be added */
);

PUB void delete_edge_list(
Edge_list_p  el);                       /* Out : the edge list         */


PUB Boolean get_edge_info(              /* Out : correct edge number   */
Edge_list_t el,                         /* In  : the edge list         */
int         index,                      /* In  : index in the list     */
Edge_p      *edge);                     /* Out : ptr to edge on 
					   position index */


PUB int nr_of_edges(                    /* Out: nr of elements         */
Edge_list_t el);                        /* In : the edge list          */

#endif /* edgelist_h */


