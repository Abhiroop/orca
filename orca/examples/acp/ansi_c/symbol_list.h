/*=====================================================================*/
/*==== symbollist.h : symbollist data structure and function       ====*/
/*====                prototypes                                   ====*/ 
/*==                                                                 ==*/

#ifndef symbol_list_h
#define symbol_list_h

#include "types.h"

/***********************************************************************
 *                                                                     *
 * The symbollist is a singly linked list of                           *
 * non-terminal symbols and is used in combination with the            *
 * dictionary data structure.                                          *
 ***********************************************************************/

/*---------------------------------------------------------------------*/
/*---- types                                                           */
/*---------------------------------------------------------------------*/

typedef struct Symbol_list_node {
    Symbol_name_t            symbol;   /* a non-terminal symbol   */
    struct Symbol_list_node  *next;    /* pointer to next element */
} Symbol_list_node_t;

typedef Symbol_list_node_t *Symbol_list_node_p;

typedef struct{
    int                 nr_of_elts;    /* nr of elements in the list   */
    Symbol_list_node_t  *head, *tail;  /* pointer to the first element */
} Symbol_list_t;

typedef Symbol_list_t *Symbol_list_p;

/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PUB Boolean create_symbol_list(	           /* Out : enough memory */
Symbol_list_p	sl);                	   /* Out : the symbollist */


PUB void delete_symbol_list(
Symbol_list_p   sl);                       /* Out : the symbollist */


PUB Boolean insert_sl_elt(                 /* Out : enough memory? */
Symbol_name_t   symbol,                    /* In  : the symbol to be added */
Symbol_list_p   sl);                       /* Out : the symbol list */


PUB void delete_sl_elt(
Symbol_name_t   symbol,                    /* In  : the symbol to be deleted */
Symbol_list_p   sl);                       /* Out : the updated symbollist */


PUB int nr_of_sl_elts(                     /* Out : number of elements */
Symbol_list_t   sl);                       /* In  : the list */


PUB Boolean give_ith_sl_elt(               /* Out : can we return ith elt? */
Symbol_list_t   sl,                        /* In  : the list */
int             index,                     /* In  : the index */
Symbol_name_t   symbol);                   /* Out : the ith element */


PUB Boolean is_in_sl(                      /* Out : is the element in the list */
Symbol_list_t   sl,                        /* In  : the list */
Symbol_name_t   symbol,                    /* In  : the symbol searched for  */
int          	*index);                   /* Out : index in the list if the *
                                            *       element is present. */

#endif /* symbol_list_h */
