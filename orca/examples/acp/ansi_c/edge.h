/*=====================================================================*/
/*==== edge.h : edge data structure and function prototypes        ====*/
/*==                                                                 ==*/

#ifndef edge_h
#define edge_h

#include "types.h"

/*---------------------------------------------------------------------*/
/*---- types                                                       ----*/
/*---------------------------------------------------------------------*/


typedef struct edge_struct{
    int                from_node;	      /* source node */
    int                to_node;		      /* destiantion node */
    int                symbol_name;           /* label of edge */        

/*
    Symbol_name_t      symbol_name;           
*/
/*
    struct edge_struct *address;            address of this edge in the list 
 */
    struct edge_struct *comp_1;               /* first consitituting component    */
    struct edge_struct *comp_2;               /* second component                 */ 

/*    Word_t             terminal;
 */
    int                  terminal;             /* if comp_1 == comp_2 == NULL then
						 this field represents the terminal
						 that this edge represenets       */
} Edge_t;

typedef Edge_t *Edge_p;



/*---------------------------------------------------------------------*/
/*---- function prototypes                                         ----*/
/*---------------------------------------------------------------------*/

/*
 @ copy_edge():  copy 2 edges 
 */

PUB void copy_edge(
Edge_p		to,			     /* In : src edge */
Edge_p    	from);                       /* Out: destination edge */
					   

/* 
 @ equal_edges: return if two edges are equal
 */

PUB Boolean equal_edges(                    /* Out: equal or not ? */
Edge_p    edge1,                            /* In : first edge */
Edge_p    edge2);                           /* In : second edge */


#endif /* edge_h */
