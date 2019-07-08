/*=====================================================================*/
/*==== nodelist.c : nodelist functions                             ====*/
/*==                                                                 ==*/

#include "nodelist.h"

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/* 
 @ create_node_list() : create a list of nodes (i.e. the chart )
 */

PUB Boolean create_node_list(	           /* Out : enough memory?        */
int           	nr_of_elts,                /* In  : nr of elements        */
Node_list_p   	nl)                        /* Out : the node list         */
{
    int         i;			   /* loop var */

    if ((nl->head = (Node_list_elt_p)malloc(nr_of_elts * sizeof(Node_list_elt_t)))
	 == (Node_list_elt_p)NULL){
	return FALSE;
    }

    nl->nr_of_elts = nr_of_elts;
    nl->tail = &(nl->head[nr_of_elts -1]);

    for (i = 0; i < nr_of_elts; i++){
	nl->head[i].elt.node_nr = i+1;
	if (!create_edge_list(&(nl->head[i].elt.incoming_edges))){
	    delete_node_list(nl);
	    return FALSE;
	}
	if (!create_edge_list(&(nl->head[i].elt.outgoing_edges))){
	    delete_edge_list(&(nl->head[i].elt.incoming_edges));
	    delete_node_list(nl);
	    return FALSE;
	}
    }
    return TRUE;
}


/* 
 @ add_incoming() : add edge to incoming edges list of node 
 */

PUB Boolean add_incoming(		   /* Out : enough memory         */
int		node_nr,                   /* In  : node nr               */
Edge_p       	edge,                      /* In  : the new incoming edge */
Node_list_p  	nl                         /* Out : updated node list     */
)
{
    add_edge(&(nl->head[node_nr - 1].elt.incoming_edges), edge);
    return TRUE;
}


/* 
 @ add_outgoing() : add edge to outgoing edges list of node 
 */

PUB Boolean add_outgoing(                  /* Out : enough memory         */
int             node_nr,                   /* In  : node nr               */
Edge_p          edge,                      /* In  : the new outgoing edge */
Node_list_p 	nl                         /* Out : updated node list     */
)
{
    add_edge(&(nl->head[node_nr - 1].elt.outgoing_edges), edge);
    return TRUE;
}

    
/* 
 @ delete_node_list() : Delete all elements of the node_list and the node_list itself
 */

PUB void delete_node_list(
Node_list_p	nl)                        /* Out : the job to be deleted */
{
    free((char *)nl->head);
}


/*
 @ get_node_info() : get information on a node 
 */

PUB Boolean get_node_info(                /* Out : correct index         */
Node_list_t	nl,	   	          /* In  : node list             */
int             index,                    /* In  : index in the list     */
Node_p          node)                     /* Out : the node_info         */
{
    if ((index < 1) || (index > nl.nr_of_elts))
	return FALSE;
    
    *node = nl.head[index-1].elt;
    return TRUE;
    
}

/* 
 @ nr_of_nodes() : return the number of elements in the node_list
 */

PUB int nr_of_nodes(                     /* Out: nr of elements         */
Node_list_t  nl                        /* In : the node_list          */
)
{
    return nl.nr_of_elts;
}
