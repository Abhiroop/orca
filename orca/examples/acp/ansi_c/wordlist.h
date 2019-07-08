/*=====================================================================*/
/*==== wordlist.h : definition of word list data structure         ====*/
/*====               and function prototypes                       ====*/
/*==                                                                 ==*/

#ifndef wordlist_h
#define wordlist_h

#include "types.h"

/***********************************************************************
 *                                                                     *
 * The wordlist is a singly linked list of                             *
 * non-terminal symbols that expand into terminals.                    *
 * The list is sorted lexicographicaly                                 *
 *                                                                     *
 ***********************************************************************/

/*---------------------------------------------------------------------*/
/*---- types                                                       ----*/
/*---------------------------------------------------------------------*/

typedef struct Wordlist_node {
    Word_t            word;         /* a terminal                      */
    struct Wordlist_node  *next;
} Wordlist_node_t;

typedef Wordlist_node_t  *Wordlist_node_p;


typedef struct{
    int              nr_of_elts;    /* nr of elements in the list      */
    Wordlist_node_t  *head, *tail;  /* pointer to the first element    */
} Wordlist_t;

typedef Wordlist_t    *Wordlist_p;


/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PUB Boolean create_wordlist(		   /* Out : enough memory */
Wordlist_p   	wl);                       /* Out : the wordlist  */


PUB void delete_wordlist(
Wordlist_p   	wl);                       /* Out : the wordlist */

PUB Boolean insert_wl_elt(		   /* Out : enough memory? */
Word_t       	word,	                   /* In  : the word to be added */
Wordlist_p   	wl);   	                   /* Out : the wordlist */

PUB void delete_wl_elt(
Word_t 		word,                      /* In  : the word to be deleted */
Wordlist_p 	wl);                       /* Out : the updated wordlist */

PUB int nr_of_wl_elts(                     /* Out : number of elements */
Wordlist_t   	wl);                       /* In  : the list */

PUB Boolean give_ith_wl_elt(               /* Out : can we return ith elt? */
Wordlist_t   	wl,                        /* In  : the list */
int          	index,                     /* In  : the index */
Word_t       	word);                     /* Out : the ith element */

PUB Boolean is_in_wl(                      /* Out : is the element in the list */
Wordlist_t   	wl,                        /* In  : the list */
Word_t          word,                      /* In  : the word searched for */
int             *index);                   /* Out : index in the list if the  *
                                            *       element is present. */
#endif
