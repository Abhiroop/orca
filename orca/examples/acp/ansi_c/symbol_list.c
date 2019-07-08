/*=====================================================================*/
/*==== symbollist.c : symbollist functions                         ====*/
/*==                                                                 ==*/

#include <stdio.h>
#include <strings.h>
#include "types.h"
#include "symbol_list.h"

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/*
 @ create_symbol_list() : create a symbol list 
 */

PUB Boolean create_symbol_list(  	   /* Out : enough memory             */
Symbol_list_p	sl)                 	   /* Out : the symbollist            */
{
    sl->head = NULL;
    sl->tail = NULL;
    sl->nr_of_elts = 0;

    return TRUE;
}

  
/*
 @ delete_symbol_list() :  delete the symbol list
 */

PUB void delete_symbol_list(
Symbol_list_p   sl)    	             	   /* Out : the symbollist            */
{
    Symbol_list_node_p old_node;	   /* pointer tp node to be deleted */
    int                i;		   /* loop var */

    for (i = 1; i <= sl->nr_of_elts; i++){
        old_node = sl->head;
        sl->head = sl->head->next;
        free((char *)old_node);
    }
}


/*
 @ insert_sl_elt(): insert element in sybol list
 */

PUB Boolean insert_sl_elt(		   /* Out : enough memory?            */
Symbol_name_t	symbol,                    /* In  : the symbol to be added    */
Symbol_list_p   sl)                        /* Out : the symbol list           */
{
    Symbol_list_node_p new_elt, ptr;       /* pointer to element in list */
    int                cmp;

    if ((new_elt = (Symbol_list_node_p)malloc(sizeof(Symbol_list_node_t)))
	 == (Symbol_list_node_p)NULL){
        return FALSE;
    }

    new_elt->next = NULL;
    strncpy(new_elt->symbol, symbol, MAX_SYMBOL_LENGTH);

    /* is the list empty ? */
    if (sl->nr_of_elts == 0){
        sl->head = new_elt;
        sl->tail = new_elt;
        sl->tail->next = NULL;
        sl->nr_of_elts++;
        return TRUE;
    }

    ptr = sl->head;
    /* should we insert as the first element? */
    cmp = strncmp(ptr->symbol, new_elt->symbol, MAX_SYMBOL_LENGTH);
    if (cmp >= 0){
	if (cmp == 0) return TRUE;  /* do not allow double elements */
        new_elt->next = sl->head;
        sl->head = new_elt;
        sl->nr_of_elts++;
        return TRUE;
    }
    
    /* insert at correct position */
    while (ptr->next != NULL){
        /* if the next element is larger or equal */
        cmp = strncmp(ptr->next->symbol, symbol, MAX_SYMBOL_LENGTH);
        if (cmp >= 0){
	    if (cmp == 0) return TRUE; /* do not allow double elements */
            new_elt->next = ptr->next;
            ptr->next = new_elt;
            sl->nr_of_elts++;
            return TRUE;
        }
        ptr = ptr->next;
    }

    /* insert as new tail */
    sl->tail->next = new_elt;
    sl->tail = new_elt;
    sl->nr_of_elts++;

    return TRUE;
}


/*
 @ delete_sl_elt() : delete element from symbol list
 */

PUB void delete_sl_elt(
Symbol_name_t	symbol,			   /* In  : the symbol to be deleted  */
Symbol_list_p   sl)                        /* Out : the updated symbollist    */
{
    fprintf(stderr, ">>>>Delete_sl_elt not implemented yet. Necessary? \n");
}


/*
 @ nr_of_sl_elts() : return number of elements in symbol list
 */
PUB int nr_of_sl_elts(                    /* Out : number of elements        */
Symbol_list_t   sl)                       /* In  : the list                  */
{
    return sl.nr_of_elts;
}


/*
 @ give_ith_sl_elt() : return i-th element from symbol list
 */

PUB Boolean give_ith_sl_elt(		   /* Out : can we return ith elt?    */
Symbol_list_t   sl,                        /* In  : the list                  */
int             index,                     /* In  : the index                 */
Symbol_name_t   symbol)                    /* Out : the ith element           */
{
    Symbol_list_node_p ptr;		   /* pointer to symbol list element */
    int                cnt;		   /* loop var */

    if ((index < 1) || (index > sl.nr_of_elts))
        return FALSE;

    ptr = sl.head;
    for (cnt = 1; cnt < index ; cnt++){
        ptr = ptr->next;
    }

    strncpy(symbol, ptr->symbol, MAX_SYMBOL_LENGTH);
    return TRUE;
}


/*
 @ is_in_sl() : check if an element is in the symbol list
 */

PUB Boolean is_in_sl(      	          /* Out : is the element in the list */
Symbol_list_t	sl,                 	  /* In  : the list                  */
Symbol_name_t   symbol,             	  /* In  : the symbol searched for   */
int             *index)            	  /* Out : index in the list if the  *
                                    	   *       element is present.       */
{
    Symbol_list_node_p 	ptr;		  /* pointer to symbol list element */
    int                	i;		  /* loop var */
    int                 cmp;              /* result of comparision */

    ptr = sl.head;
    for (i=1; i <= sl.nr_of_elts; i++){
	cmp = strncmp(ptr->symbol, symbol, MAX_SYMBOL_LENGTH);

	/* list is sorted, so check if we can break */
	if (cmp > 0){
	    return FALSE;
	}

        if (cmp == 0){
            *index = i;
            return TRUE;
        }
        ptr = ptr->next;
    }
    return FALSE;
}
