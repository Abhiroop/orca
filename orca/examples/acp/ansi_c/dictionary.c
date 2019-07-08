/*=====================================================================*/
/*==== dictionary.c : implementation of dictionary functions       ====*/
/*==                                                                 ==*/

#include <stdio.h>
#include <strings.h>
#include "types.h"
#include "dictionary.h"

/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

					   
PRIV Boolean dict_add_new_elt(		   /* Out : enough memory? */
Word_t          word,                      /* In  : the word to be added */
Symbol_name_t   non_terminal,              /* In  : produced by this non-terminal */
Dictionary_p    dict);                     /* Out : the dictionary */


/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/*
 @ create_dict() : create the dictionary 
 */

PUB Boolean create_dict(            	   /* Out : enough memory */
Dictionary_p   dict)                       /* Out : the dictionary */
{
    dict->head = NULL;
    dict->tail = NULL;
    dict->nr_of_elts = 0;

    return TRUE;
}


/*
 @ delete_dict() : delete the dictionary
 */

PUB void delete_dict(
Dictionary_p    dict)			   /* Out : the wordlist */
{
    Dict_node_p     old_node;
    int             i;

    for (i = 1; i <= dict->nr_of_elts; i++){
        old_node = dict->head;
        dict->head = dict->head->next;
	delete_symbol_list(&(old_node->symbol_list));
        free((char *)old_node);
    }

}


/* 
 @  insert_dict_word() : insert a word in the dictionary 
 */

PUB Boolean insert_dict_word(   	   /* Out : enough memory? */
Word_t          word,                      /* In  : the word to be added */
Symbol_name_t   non_terminal,              /* In  : produced by this non-terminal */
Dictionary_p    dict)                      /* Out : the dictionary */
{
    Dict_node_p ptr;			   /* pointer to dictionary element */
    int         index;			   /* index in dictionary */
    int         i; 			   /* loop var */

    /* if word is present */
    if (is_in_dict(*dict, word, &index)){

	/* add to its symbol list */
	ptr = dict->head;
	for (i = 1; i < index ; i++)
	    ptr = ptr->next;

	return insert_sl_elt(non_terminal, &(ptr->symbol_list));
    }

    /* else insert as a new element */
    return dict_add_new_elt(word, non_terminal, dict);
}


/*
 @ add_new_elt() : add new element to dictionary 
 */

PRIV Boolean dict_add_new_elt(		   /* Out : enough memory? */
Word_t          word,    	           /* In  : the word to be added */
Symbol_name_t   non_terminal,              /* In  : produced by this non-terminal */
Dictionary_p    dict)                      /* Out : the dictionary */
{
    Dict_node_p  new_elt, ptr;             /* pointers to dictionary elements */

    if ((new_elt = (Dict_node_p)malloc(sizeof(Dict_node_t))) == (Dict_node_p)NULL){
	return FALSE;
    }

    if (!create_symbol_list(&(new_elt->symbol_list))){
	free((char*)new_elt);
	return FALSE;
    }

    if (!insert_sl_elt(non_terminal, &(new_elt->symbol_list))){
	delete_symbol_list(&(new_elt->symbol_list));
	free((char*)new_elt);
	return FALSE;
    }
    
    new_elt->next = NULL;
    strncpy(new_elt->word, word, MAX_WORD_LENGTH);

    /* is the list empty ? */
    if (dict->nr_of_elts == 0){
        dict->head = new_elt;
        dict->tail = new_elt;
        dict->tail->next = NULL;
        dict->nr_of_elts++;
        return TRUE;
    }
    
    ptr = dict->head;
    /* should we insert as the first element? */
    if (strncmp(ptr->word, new_elt->word, MAX_WORD_LENGTH) >= 0){
        new_elt->next = dict->head;
        dict->head = new_elt;
        dict->nr_of_elts++;
        return TRUE;
    }

    /* insert at correct position */
    while (ptr->next != NULL){
        /* if the next element is larger or equal */
        if (strncmp(ptr->next->word, word, MAX_WORD_LENGTH) >= 0){
            new_elt->next = ptr->next;
            ptr->next = new_elt;
            dict->nr_of_elts++;
            return TRUE;
        }
	ptr = ptr->next;
    }   

    /* insert as new tail */
    dict->tail->next = new_elt;
    dict->tail = new_elt;
    dict->nr_of_elts++;

    return TRUE;
}


/*
 @ delete_dict_word() : delete word from dictionary
 */

PUB void delete_dict_word(
Word_t          word,			   /* In  : the word to be deleted */
Dictionary_p    dict)                      /* Out : the updated dictionary */
{
    fprintf(stderr, ">>>>Delete_dict_elt not implemented yet. Necessary? \n");
}


/* 
 @ nr_of_dict_words() : nr of words in dictionary
 */

PUB int nr_of_dict_words(		   /* Out : number of words */
Dictionary_t    dict)                      /* In  : the list */
{
    return dict.nr_of_elts;
}


/*
 @ give_word_info() : give information on word from the dictionary
 */

PUB Boolean give_word_info(		   /* Out : can we return ith word info? */
Dictionary_t    dict,                      /* In  : the dictionary */
int             index,                     /* In  : the index */
Word_t          word,                      /* Out : word on poistion index */
Symbol_list_p   symbol_list)               /* Out : the list of symbols that */
                                           /*       can produce the index-th word */
                                           /*       in the dictionary */
{
    Dict_node_p ptr;
    int         cnt;

    if ((index < 1) || (index > dict.nr_of_elts))
        return FALSE;

    ptr = dict.head;
    for (cnt = 1; cnt < index ; cnt++){
        ptr = ptr->next;
    }

    (void)strncpy(word, ptr->word, MAX_WORD_LENGTH);
    *symbol_list = ptr->symbol_list;
    return TRUE;
}


/* 
 @ is_in_dict() : see if a word is in the dictionary 
 */

PUB Boolean is_in_dict(		           /* Out : is the element in the list */
Dictionary_t    dict,                      /* In  : the list */
Word_t          word,                      /* In  : the word searched for */
int             *index)                    /* Out : index in the list if the 
                                            *       element is present. */
{
    Dict_node_p  ptr;
    int          i;
    
    ptr = dict.head;
    for (i=1; i <= dict.nr_of_elts; i++){
        if (strncmp(ptr->word, word, MAX_WORD_LENGTH) == 0){
            *index = i;
            return TRUE;
        }
        ptr = ptr->next;
    }
    return FALSE;
}    
