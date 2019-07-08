/*=====================================================================*/
/*==== grammar.c : implementation of grammar functions             ====*/
/*==                                                                 ==*/

#include <stdio.h>
#include <strings.h>
#include "types.h"
#include "grammar.h"

/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PRIV Boolean grammar_add_new_rule(         /* Out : enough memory? */
Rule_t          rule,                      /* In  : the rule to add */
Grammar_p       grammar);                  /* Out : the dictionary */

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/


/*
 @ create_grammar() : create a grammar
 */

PUB Boolean create_grammar(                /* Out : enough memory */
Grammar_p       grammar)              	   /* Out : the grammar */
{
    grammar->head = NULL;
    grammar->tail = NULL;
    grammar->nr_of_elts = 0;
    
    return TRUE;
}


/* 
 @ delete_grammar() : delete the grammar
 */
PUB void delete_grammar(
Grammar_p       grammar)              	   /* Out : the grammar */
{
					   
    Gramm_elt_p old_node;		   /* pointer to element */
    int         i;			   /* loop var */

    for (i = 1; i <= grammar->nr_of_elts; i++){
        old_node = grammar->head;
        grammar->head = grammar->head->next;
	delete_comb_list(&(old_node->comb_list));
        free((char *)old_node);
    }
}    
    

/*
 @ insert_prod_rule
 */

PUB Boolean insert_prod_rule(              /* Out : enough memory? */
Rule_t         	rule,                      /* In  : the production rule */
Grammar_p       grammar)                   /* Out : the grammar */
{

    Gramm_elt_p ptr;			   /* pointer to element */
    int 	index;			   /* index in grammar */
    int		i;			   /* loop var */

    /* if symbol is present */
    if (is_in_grammar(*grammar, rule.right1, &index)){

        /* add to its symbol list */
        ptr = grammar->head;
        for (i = 1; i < index ; i++)
            ptr = ptr->next;

        return insert_comb(rule.right2, rule.new, &(ptr->comb_list));
    }

    /* else insert as a new element */
    return grammar_add_new_rule(rule, grammar);
}
    

/*
 @ grammar_add_new_rule() : add new rule to grammar
 */

PRIV Boolean grammar_add_new_rule(      /* Out : enough memory? */
Rule_t		rule,                     /* In  : the rule to add */
Grammar_p       grammar)                  /* Out : the dictionary */
{
    Gramm_elt_p new_elt, ptr;		  /* pointers to gramar elements */

    if ((new_elt = (Gramm_elt_p)malloc(sizeof(Gramm_elt_t))) == (Gramm_elt_p)NULL){
        return FALSE;
    }

    if (!create_comb_list(&(new_elt->comb_list))){
        free((char*)new_elt);
        return FALSE;
    }

    if (!insert_comb(rule.right2, rule.new, &(new_elt->comb_list))){
        delete_comb_list(&(new_elt->comb_list));
        free((char*)new_elt);
        return FALSE;
    }

    new_elt->next = NULL;
    strncpy(new_elt->from_symbol, rule.right1, MAX_SYMBOL_LENGTH);
    
    /* is the list empty ? */
    if (grammar->nr_of_elts == 0){
        grammar->head = new_elt;
        grammar->tail = new_elt;
        grammar->tail->next = NULL;
        grammar->nr_of_elts++;
        return TRUE;
    }
    

    ptr = grammar->head;
    /* should we insert as the first element? */
    if (strncmp(ptr->from_symbol, new_elt->from_symbol, MAX_SYMBOL_LENGTH) >= 0){
        new_elt->next = grammar->head;
        grammar->head = new_elt;
        grammar->nr_of_elts++;
        return TRUE;
    }

    /* insert at correct position */
    while (ptr->next != NULL){
        /* if the next element is larger or equal */
        if (strncmp(ptr->next->from_symbol, rule.right1, MAX_WORD_LENGTH) >= 0){
            new_elt->next = ptr->next;
            ptr->next = new_elt;
            grammar->nr_of_elts++;
            return TRUE;
        }
        ptr = ptr->next;
    }   

    /* insert as new tail */
    grammar->tail->next = new_elt;
    grammar->tail = new_elt;
    grammar->nr_of_elts++;

    return TRUE;
}


/*
 @ delete_prod_rule() : delete element from grammar
 */

PUB void delete_prod_rule(
Symbol_name_t   symbol_name,		   /* In  : key  */
Grammar_p       grammar)                   /* Out : the updated dictionary */
{
    fprintf(stderr, ">>>>Delete_prod_rule not implemented yet. Necessary? \n");
}
    

/*
 @ nr_of_rules() : return nr of rules in the grammar
 */

PUB int nr_of_rules(			   /* Out : number of production rules */
Grammar_t    	grammar)                   /* In  : the list                  */
{
    return grammar.nr_of_elts;
}


/*
 @ give_rule_info() : give information on a rule in the grammar 
 */

PUB Boolean give_rule_info( 	          /* Out : can we return ith production */
			                  /*       rule info?                */
Grammar_t       grammar,                  /* In  : the grammar               */
int             index,                    /* In  : the index                 */
Symbol_name_t   symbol,                   /* Out : the symbol on position index  */
Comb_list_p     comb_list)                /* Out : the list of combinations  */
{
    Gramm_elt_p ptr;
    int         cnt;

    if ((index < 1) || (index > grammar.nr_of_elts))
        return FALSE;

    ptr = grammar.head;
    for (cnt = 1; cnt < index ; cnt++){
        ptr = ptr->next;
    }

    (void)strncpy(symbol, ptr->from_symbol, MAX_SYMBOL_LENGTH);
    *comb_list = ptr->comb_list;
    return TRUE;
}


/*
 @ is_in_grammar() : check is a rule is in the grammar
 */

PUB Boolean is_in_grammar(          	 /* Out : is the element in the list */
Grammar_t      grammar,                  /* In  : the list */
Symbol_name_t  symbol,                   /* In  : the symbol searched for */
int            *index)                   /* Out : index in the list if the *
                                          *       element is present. */
{
    Gramm_elt_p  ptr;                    /* pointer to element in list */
    int          i;                      /* loop var */
    int          cmp;                    /* result of comparision */
    
    
    ptr = grammar.head;
    for (i=1; i <= grammar.nr_of_elts; i++){
	cmp = strncmp(ptr->from_symbol, symbol, MAX_SYMBOL_LENGTH);
	
	/* list is sorted, so check if we can break */
	if (cmp > 0) {
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
