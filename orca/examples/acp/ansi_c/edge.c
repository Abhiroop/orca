/*=====================================================================*/
/*==== edge.c : implementation of edge functions                   ====*/
/*==                                                                 ==*/

#include <strings.h>
#include "types.h"
#include "edge.h"

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/*
 @ copy_edge():  copy 2 edges 
 */

PUB void copy_edge(
Edge_p    to,                               /* In : src edge */
Edge_p    from)                             /* Out: destination edge */
{
    to->from_node = from->from_node;
    to->to_node   = from->to_node;
/*
    to->address   = from.address;
*/
    to->comp_1 = from->comp_1;
    to->comp_2 = from->comp_2;

/*
    strncpy(to->symbol_name, from.symbol_name, MAX_SYMBOL_LENGTH);
    strncpy(to->terminal, from.terminal, MAX_WORD_LENGTH);
*/

    to->symbol_name = from->symbol_name;
    to->terminal = from->terminal;
    
    return;
}

/* 
 @ equal_edges: return if two edges are equal
 */

PUB Boolean equal_edges(                    /* Out : equal ? */
Edge_p    edge1,                            /* In : first edge */
Edge_p    edge2)                            /* In : second edge */
{
    /* edges are equal if their fields are equal */

    /* NOTE : SHOULD THIS BE SYMMETRIC ?? */

    return 
	   (edge1->comp_1 == edge2->comp_1) &&
	   (edge1->comp_2 == edge2->comp_2) &&
           (edge1->from_node == edge2->from_node) &&
	   (edge1->to_node   == edge2->to_node) &&
	   (edge1->symbol_name == edge2->symbol_name);

/*
	   (strncmp(edge1->symbol_name, edge2->symbol_name, MAX_SYMBOL_LENGTH) == 0);
*/
}

