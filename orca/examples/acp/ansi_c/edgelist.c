/*=====================================================================*/
/*==== edgelist.c : edgelist functions                             ====*/
/*==                                                                 ==*/

#include "edgelist.h"

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/*
 @ create_edge_list() : create a list of edges 
 */

PUB Boolean create_edge_list(              /* Out : enough memory?        */
Edge_list_p	el)		           /* Out : the edge list         */
{
    el->head = NULL;
    el->tail = NULL;
    el->nr_of_elts = 0;
}


/* 
 @ add_edge() : Add the element at the tail of the queue
 */
PUB Boolean add_edge(                      /* Out : enough memeory?       */
Edge_list_p	el,                        /* Out : the jobqueue          */
Edge_p       	elt                        /* In  : The element to be added */
)
{
    Edge_list_elt_p new_elt;               /* pointer to new element */

    if ((new_elt = (Edge_list_elt_p)malloc(sizeof(Edge_list_elt_t))) ==
	 (Edge_list_elt_p)NULL){
	return FALSE;
    }
    new_elt->elt = elt;
    new_elt->next = (Edge_list_elt_p)NULL;

    if (el->nr_of_elts == 0)
	el->head = new_elt;
    else
	el->tail->next =  new_elt;

    el->tail = new_elt;
    el->nr_of_elts++;

    return TRUE;
}
    

/* 
 @ delete_edge_list() : Delete all elements of the Edge_list and the queue itself
 */

PUB void delete_edge_list(
Edge_list_p	el)			   /* Out : the edge list         */
{
    Edge_list_elt_p old_node;
    int             i;

    for (i = 1; i <= el->nr_of_elts; i++){
        old_node = el->head;
        el->head = el->head->next;
        free((char *)old_node);
    }
}


/*
 @ get_edge_info() : get information on element of edge list 
 */

PUB Boolean get_edge_info(                /* Out : correct edge number   */
Edge_list_t	el,                       /* In  : the edge list         */
int         	index,                    /* In  : index in the list     */
Edge_p      	*edge)                    /* Out : edge on position index */
{
    Edge_list_elt_p ptr;
    int             cnt;

    if ((index < 1) || (index > el.nr_of_elts))
	return FALSE;

    ptr = el.head;
    for (cnt = 1; cnt < index ; cnt++){
        ptr = ptr->next;
    }

    *edge = ptr->elt;

    return TRUE;
}


/* 
 @ nr_of_edge() : return nr of edges in an edgelist
 */

PUB int nr_of_edges(                    /* Out: nr of elements         */
Edge_list_t 	el)                     /* In : the edge list          */
{
    return el.nr_of_elts;
}

