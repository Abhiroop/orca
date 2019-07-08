/*=====================================================================*/
/*==== comb_list.c : combination list functions implementation     ====*/
/*==                                                                 ==*/

#include <stdio.h>
#include <strings.h>
#include "types.h"
#include "comb_list.h"

/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PRIV Boolean add_new_comb(  
Symbol_name_t	comb_symbol, 
Symbol_name_t   new_symbol, 
Comb_list_p     cl);       
		
/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/*
 @ create_comb_list() : create a combination list 
 */

PUB Boolean create_comb_list(   	   /* Out : enough memory */
Comb_list_p     cl)                        /* Out : the combination list */
					   
{
    cl->head = NULL;
    cl->tail = NULL;
    cl->nr_of_elts = 0;
    
    
    return TRUE;
}


/*
 @ delete_comb_list() : delete the combination list 
 */

PUB void delete_comb_list(
Comb_list_p     cl)			   /* Out : the combination list */
{
    CL_elt_p 	old_node;                  /* pointer to node to be deleted */
    int         i;          		   /* loop index */

    for (i = 1; i <= cl->nr_of_elts; i++){
        old_node = cl->head;
        cl->head = cl->head->next;
	delete_symbol_list(&(old_node->sl));
        free((char *)old_node);
    }
    
}


/*
 @ insert_comb() : insert an element in the combination list
 */

PUB Boolean insert_comb(                   /* Out : enough memory? */
Symbol_name_t	comb_symbol,               /* In  : the combination symbol (R2) */
Symbol_name_t  	new_symbol,                /* In  : the new lhs symbol (L) */
Comb_list_p     cl)                        /* Out : the combination list */
{
    CL_elt_p 	ptr;                       /* pointer to list element */
    int      	i;                         /* loop variable */
    int         index;                     /* index in combination list */

    /* check if 'comb_symbol' is in the combination list. If so, add to 
     * the corresponding symbol_list 
     */

    if (is_in_comb_list(*cl, comb_symbol, &index)){
	ptr = cl->head;
	for (i = 1; i < index ; i++){
	    ptr = ptr->next;
	}
	return insert_sl_elt(new_symbol, &(ptr->sl));
    }

    /* nope, it's not in the list; add it */
    return add_new_comb(comb_symbol, new_symbol, cl);
}


/* 
 @ add_new_comb() : add new element to combination list 
 */

PRIV Boolean add_new_comb(  		   /* Out : enough memory? */
Symbol_name_t	comb_symbol,               /* In  : the combination symbol (R2) */
Symbol_name_t  	new_symbol,                /* In  : the new lhs symbol (L) */
Comb_list_p     cl)                        /* Out : the combination list */
{
 
    CL_elt_p    new_elt, ptr;		   /* pointer to elements in list */

    if ((new_elt = (CL_elt_p)malloc(sizeof(CL_elt_t))) == (CL_elt_p)NULL){
	return FALSE;
    }

    if (!create_symbol_list(&(new_elt->sl))){
	free((char*)new_elt);
	return FALSE;
    }
    
    if (!insert_sl_elt(new_symbol, &(new_elt->sl))){
	delete_symbol_list(&(new_elt->sl));
	free((char*)new_elt);
	return FALSE;
    }

    new_elt->next = NULL;
    strncpy(new_elt->combination_symbol, comb_symbol, MAX_SYMBOL_LENGTH);

    /* is the list empty ? */
    if (cl->nr_of_elts == 0){
        cl->head = new_elt;
        cl->tail = new_elt;
        cl->tail->next = NULL;  
        cl->nr_of_elts++; 
        return TRUE;
    }

    ptr = cl->head;
    /* should we insert as the first element? */
    if (strncmp(ptr->combination_symbol, comb_symbol, MAX_SYMBOL_LENGTH) >= 0){
        new_elt->next = cl->head;
        cl->head = new_elt;
        cl->nr_of_elts++; 
        return TRUE;
    }
    
    /* insert at correct position */
    while (ptr->next != NULL){
        /* if the next element is larger or equal */
        if (strncmp(ptr->next->combination_symbol, comb_symbol, MAX_SYMBOL_LENGTH) >= 0){
            new_elt->next = ptr->next;
            ptr->next = new_elt;        
            cl->nr_of_elts++;       
            return TRUE;
        }
        ptr = ptr->next;
    }

    /* insert as new tail */
    cl->tail->next = new_elt;
    cl->tail = new_elt;
    cl->nr_of_elts++;       

    return TRUE;
}
    

/*
 @ delete_comb() : delete element from the list 
 */

PUB void delete_comb(
Symbol_name_t   comb_symbol,		   /* In  : see combination symbol */
Comb_list_p     cl)                        /* Out : the combination list */
		
{
    fprintf(stderr, ">>>>Delete_comb not implemented yet. Necessary? \n");
}    


/*
 @ nr_of_combinations() : dreturn nr of elements
 */

PUB int nr_of_combinations(		   /* Out : number of combinations */
Comb_list_t    	cl)                        /* In  : the combination list */
{
    return cl.nr_of_elts;
}


/* 
 @ give_comb_info() m: return information on an element from the list 
 */
 
PUB Boolean give_comb_info(                /* Out : can we return ith */
			           	   /*       combination info? */
Comb_list_t     cl,                        /* In  : the combination_list */
int             index,                     /* In  : the index */
Symbol_name_t   comb_symbol,               /* Out : the combination symbol (R2) */
Symbol_list_p   sl)                        /* Out : list of new symbols (L)   */
                                           /*       that can be produced with */
                                           /*       righthand symbols R1 & R2 */
{
    CL_elt_p ptr;
    int      cnt;

    if ((index < 1) || (index > cl.nr_of_elts))
        return FALSE;

    ptr = cl.head;
    for (cnt = 1; cnt < index ; cnt++){
        ptr = ptr->next;
    }
    
    strncpy(comb_symbol, ptr->combination_symbol, MAX_SYMBOL_LENGTH);
    *sl = ptr->sl;
    return TRUE;
}
    

/*
 @ is_in_comb_list() : check if a combination is in the list 
 */

PUB Boolean is_in_comb_list(		   /* Out : is the element in the list */
Comb_list_t     cl,                        /* In  : the list */
Symbol_name_t   comb_symbol,               /* In  : the combination symbol */
			                   /*       searched for */
int             *index)                    /* Out : index in the list if the  *
                                            *       element is present. */
{
    CL_elt_p    ptr;			   /* pointer to list element */
    int         i;                         /* loop variabele */
    int         cmp;                       /* result of comparision */
    
    ptr = cl.head;
    for (i=1; i <= cl.nr_of_elts; i++){
	cmp = strncmp(ptr->combination_symbol, comb_symbol, MAX_SYMBOL_LENGTH);

	/* list is sorted, so check if we cab break */
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
