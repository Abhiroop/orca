/*=====================================================================*/
/*==== comb_list.h : definition of combination list data structure ====*/
/*====               and function prototypes                       ====*/
/*==                                                                 ==*/

#ifndef comb_list_h
#define comb_list_h

#include "types.h"
#include "symbol_list.h"

/***********************************************************************
 *                                                                     *
 * comb_list is a data structure of a linked list that stores lists of *
 * rules. It is used in combination with the grammar data              *
 * structure in the following way: if element R1 of the grammar list   *
 * has combination list CL, then element R2 of CL has a list SL that   *
 * stores all symbols L such that L -> R1 R2 is a rule in the grammar  *
 *                                                                     *
 ***********************************************************************/

/*---------------------------------------------------------------------*/
/*---- types                                                       ----*/
/*---------------------------------------------------------------------*/

typedef struct CL_elt {
    Symbol_name_t combination_symbol; /* the combinations symbol (R2)  */
    Symbol_list_t sl;                 /* list of new symbols           */
    struct CL_elt *next;              /* pointer to  */
} CL_elt_t;
    
typedef CL_elt_t *CL_elt_p;

typedef struct {
    int           nr_of_elts;         /* nr of elements in the grammar */
    CL_elt_t      *head, *tail;       /* head an tail of the list      */
} Comb_list_t;

typedef Comb_list_t *Comb_list_p;

/*---------------------------------------------------------------------*/
/*---- function prototypes                                         ----*/
/*---------------------------------------------------------------------*/

PUB Boolean create_comb_list(       /* Out : enough memory             */
Comb_list_p    cl);                 /* Out : the combination list      */


PUB void delete_comb_list(
Comb_list_p    cl);                 /* Out : the combination list      */


PUB Boolean insert_comb(            /* Out : enough memory?            */
Symbol_name_t  comb_symbol,         /* In  : the combination symbol (R2) */
Symbol_name_t  new_symbol,          /* In  : the new lhs symbol (L)    */
Comb_list_p    cl);                 /* Out : the combination list      */


PUB void delete_comb(
Symbol_name_t  comb_symbol,         /* In  : see combination symbol    */
Comb_list_p    cl);                 /* Out : the combination list      */


PUB int nr_of_combinations(         /* Out : number of combinations    */
Comb_list_t    cl);                 /* In  : the combination list      */


PUB Boolean give_comb_info(         /* Out : can we return ith         */
			            /*       combination info?         */
Comb_list_t    cl,                  /* In  : the combination_list      */
int            index,               /* In  : the index                 */
Symbol_name_t  comb_symbol,         /* Out : the combination symbol (R2) */
Symbol_list_p  sl);                 /* Out : list of new symbols (L)   */
                                    /*       that can be produced with */
                                    /*       righthand symbols R1 & R2 */


PUB Boolean is_in_comb_list(        /* Out : is the element in the list */
Comb_list_t    cl,                  /* In  : the list                  */
Symbol_name_t  comb_symbol,         /* In  : the combination symbol    */
			            /*       searched for              */
int            *index);             /* Out : index in the list if the  *
                                     *       element is present.       */

#endif /* comb_list_h */
