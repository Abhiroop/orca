/*=====================================================================*/
/*==== wordlist.c : wordlist functions and implementation          ====*/
/*==                                                                 ==*/

#include <stdio.h>
#include <strings.h>
#include "wordlist.h"

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/

/*
 @ create_word_list() : create a word list  
 */

PUB Boolean create_wordlist(		   /* Out : enough memory */
Wordlist_p	wl)                        /* Out : the wordlist */
{
    wl->head = NULL;
    wl->tail = NULL;
    wl->nr_of_elts = 0;
    
    return TRUE;
}


/*
 @ delete_word_list() : delete the wordlist
 */

PUB void delete_wordlist(
Wordlist_p   	wl)                        /* Out : the wordlist */
{
    Wordlist_node_p old_node;		   /* pointer to element in list */
    int             i;			   /* loop var */

    for (i = 1; i <= wl->nr_of_elts; i++){
	old_node = wl->head;
	wl->head = wl->head->next;
	free((char *)old_node);
    }
    
}


/*
 @ insert_wl_elt() : insert word in wordlist
 */

PUB Boolean insert_wl_elt( 		   /* Out : enough memory? */
Word_t       	word,                      /* In  : the word to be added */
Wordlist_p   	wl)                        /* Out : the wordlist */
{
    Wordlist_node_p new_elt, ptr;	   /* pointers to wordlist elts */
    
    if ((new_elt = (Wordlist_node_p)malloc(sizeof(Wordlist_node_t))) == (Wordlist_node_p)NULL){
	return FALSE;
    }

    new_elt->next = NULL;
    strncpy(new_elt->word, word, MAX_WORD_LENGTH);

    /* is the list empty ? */
    if (wl->nr_of_elts == 0){
	wl->head = new_elt;
	wl->tail = new_elt;
	wl->tail->next = NULL;	
	wl->nr_of_elts++; 
	return TRUE;
    }

    ptr = wl->head;
    /* should we insert as the first element? */
    if (strncmp(ptr->word, new_elt->word, MAX_WORD_LENGTH) >= 0){
	new_elt->next = wl->head;
	wl->head = new_elt;
	wl->nr_of_elts++; 
	return TRUE;
    }
    
    /* insert at correct position */
    while (ptr->next != NULL){
	/* if the next element is larger or equal */
	if (strncmp(ptr->next->word, word, MAX_WORD_LENGTH) >= 0){
	    new_elt->next = ptr->next;
	    ptr->next = new_elt;	
	    wl->nr_of_elts++; 	    
	    return TRUE;
	}
	ptr = ptr->next;
    }

    /* insert as new tail */
    wl->tail->next = new_elt;
    wl->tail = new_elt;
    wl->nr_of_elts++; 	    

    return TRUE;
}


/*
 @ delete_wl_elt() : delete element from wordlist
 */

PUB void delete_wl_elt(
Word_t		word,			   /* In  : the word to be deleted */
Wordlist_p   	wl)                        /* Out : the updated wordlist */
{
    fprintf(stderr, ">>>>Delete_wl_elt not implemented yet. Necessary? \n");
}
    

/*
 @ nr_of_wl_elts() : nr of elements in wordlist
 */

PUB int nr_of_wl_elts(                     /* Out : number of elements */
Wordlist_t   	wl)                        /* In  : the list */
{
    return wl.nr_of_elts;
}


/* 
 @ give_ith_wl_elt() : give information on ith element of the wordlist
 */

PUB Boolean give_ith_wl_elt(              /* Out : can we return ith elt? */
Wordlist_t   	wl,                       /* In  : the list */
int          	index,                    /* In  : the index */
Word_t       	word)                     /* Out : the index-th element */
{
    Wordlist_node_p ptr;		  /* pointer to element in list */ 
    int 	cnt;			  /* loop var */

    if ((index < 1) || (index > wl.nr_of_elts))
	return FALSE;

    ptr = wl.head;
    for (cnt = 1; cnt < index ; cnt++){
	ptr = ptr->next;
    }
    
    strncpy(word, ptr->word, MAX_WORD_LENGTH);
    return TRUE;
}


/*
 @ is_in_wl() : check if a word is in the wordlist 
 */

PUB Boolean is_in_wl(                     /* Out : is the element in the list */
Wordlist_t   	wl,                       /* In  : the list */
Word_t       	word,                     /* In  : the word searched for */
int          	*index)                   /* Out : index in the list if the  *
                                           *       element is present. */
{
    Wordlist_node_p ptr;		  /* pointer to element in list */
    int         i;			  /* loop var */
    int         cmp;                      /* result of comparision */

    ptr = wl.head;
    for (i=1; i <= wl.nr_of_elts; i++){
	cmp = strncmp(ptr->word, word, MAX_WORD_LENGTH);

	/* list is sorted, so check if we can break */
	if (cmp > 0 ){
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

