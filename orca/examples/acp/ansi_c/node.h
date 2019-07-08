/*=====================================================================*/
/*==== node.h : node data structure                                ====*/
/*==                                                                 ==*/

#ifndef node_h
#define node_h

#include "edgelist.h"

typedef struct{
    int             node_nr;	           /* node number */
    Edge_list_t     incoming_edges;	   /* incoming edges */
    Edge_list_t     outgoing_edges;	   /* outgoing edges */
} Node_t;

typedef Node_t *Node_p;
    
#endif
