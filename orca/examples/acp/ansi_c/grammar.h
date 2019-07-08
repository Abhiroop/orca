/*=====================================================================*/
/*==== grammar.h : gramar data structure and function prototypes   ====*/
/*==                                                                 ==*/

#ifndef grammar_h
#define grammar_h

#include "types.h"
#include "symbol_list.h"
#include "comb_list.h"

/***********************************************************************
 *                                                                     *
 * The grammar is stored as follows:                                   *
 * Per non-terminal symbol NT we store a list of symbols with which NT *
 * can be combined to produce a new non-terminal symbol in the grammar.*
 * This way of storing is in fact a reversed way of storing the        *
 * grammar as represented by the production rules.                     *
 * See section 2.7 of G.V. Wilson's "Cowichan Problems" and comb_list.h*
 *                                                                     *
 ***********************************************************************/

/*---------------------------------------------------------------------*/
/*---- types                                                       ----*/
/*---------------------------------------------------------------------*/

typedef struct Grammar_elt {
    Symbol_name_t from_symbol;        /* the symbol for which combinations 
					 are stored */
    Comb_list_t        comb_list;     /* the combination list */
    struct Grammar_elt *next;         /* pointer to  */
} Gramm_elt_t;
    
typedef Gramm_elt_t *Gramm_elt_p;

typedef struct {
    int         nr_of_elts;           /* nr of elements in the grammar */
    Gramm_elt_t *head, *tail;
} Grammar_t;

typedef Grammar_t *Grammar_p;


/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PUB Boolean create_grammar(		   /* Out : enough memory */
Grammar_p	grammar);                  /* Out : the grammar */
					   
PUB void delete_grammar(
Grammar_p       grammar);                  /* Out : the grammar */


PUB void delete_prod_rule(
Symbol_name_t   symbol_name,               /* In  : see below */
Grammar_p       grammar);                  /* Out : the updated dictionary */


PUB Boolean give_rule_info(                /* Out : can we return ith production */
			                   /*       rule info? */
Grammar_t      grammar,                    /* In  : the grammar */
int            index,                      /* In  : the index */
Symbol_name_t  symbol,                     /* Out : the symbol on position index  */
Comb_list_p    comb_list);                 /* Out : the list of combinations  */


PUB Boolean insert_prod_rule(              /* Out : enough memory? */
Rule_t          rule,                      /* In  : the production rule */
Grammar_p       grammar);                  /* Out : the grammar */


PUB Boolean is_in_grammar(                 /* Out : is the element in the grammar */
Grammar_t      grammar,                    /* In  : the list */
Symbol_name_t  symbol,                     /* In  : the symbol searched for */
int            *index);                    /* Out : index in the list if the *
                                            *       element is present. */

PUB int nr_of_rules(                       /* Out : number of production rules */
Grammar_t       grammar);                  /* In  : the list */


#endif /* grammar_h */
