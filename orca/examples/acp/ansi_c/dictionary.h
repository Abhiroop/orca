/*=====================================================================*/
/*==== dictionary.h : definition of dictionary data structure and  ====*/
/*====                function prototypes                          ====*/
/*==                                                                 ==*/

#ifndef dictionary_h
#define dictionary_h

#include "types.h"
#include "symbol_list.h"

/***********************************************************************
 *                                                                     *
 * The dictionary is represented as a sorted singly linked list of     *
 * terminal. For each terminal T, a list of symbols that can procuce T *
 * is stored                                                           *
 *                                                                     *
 ***********************************************************************/

/*---------------------------------------------------------------------*/
/*---- types                                                       ----*/
/*---------------------------------------------------------------------*/

typedef struct Dict_node {
    Word_t            word;         /* this word can be produced by  */
    Symbol_list_t     symbol_list;  /* this list of non-terminals    */
    struct Dict_node  *next;
} Dict_node_t;

typedef Dict_node_t *Dict_node_p;

typedef struct{
    int          nr_of_elts;        /* nr of words in the dictionary */
    Dict_node_t  *head, *tail;      /* pointer to the first element  */
} Dictionary_t;

typedef Dictionary_t *Dictionary_p;

/*---------------------------------------------------------------------*/
/*---- function prototypes                                         ----*/
/*---------------------------------------------------------------------*/

PUB Boolean create_dict(		   /* Out : enough memory */
Dictionary_p    dict);                     /* Out : the dictionary */


PUB void delete_dict(
Dictionary_p    dict);                     /* Out : the wordlist */


PUB Boolean insert_dict_word(              /* Out : enough memory? */
Word_t          word,                      /* In  : the word to be added */
Symbol_name_t   non_terminal,              /* In  : produced by this non-terminal */
Dictionary_p    dict);                     /* Out : the dictionary */


PUB void delete_dict_word(
Word_t          word,                      /* In  : the word to be deleted */
Dictionary_p    dict);                     /* Out : the updated dictionary */


PUB int nr_of_dict_words(                  /* Out : number of words */
Dictionary_t   dict);                      /* In  : the list */


PUB Boolean give_word_info(                /* Out : can we return ith word info? */
Dictionary_t   dict,                       /* In  : the dictionary */
int            index,                      /* In  : the index */
Word_t         word,                       /* Out : word on poistion index */
Symbol_list_p  symbol_list);               /* Out : the list of symbols that  */
                                           /*       can produce the ith word  */
                                           /*       in the dictionary         */

PUB Boolean is_in_dict(                    /* Out : is the element in the list */
Dictionary_t   dict,                       /* In  : the list */
Word_t         word,                       /* In  : the word searched for     */
int            *index);                    /* Out : index in the list if the  *
                                            *       element is present.       */

#endif /* dictionary_h */
