/*=====================================================================*/
/*==== parser.h : parser function prototypes                       ====*/
/*==                                                                 ==*/

#ifndef parser_h
#define parser_h

#include <stdio.h>
#include "types.h"
#include "grammar.h"
#include "dictionary.h"
#include "symbol_list.h"

/*---------------------------------------------------------------------*/
/*---- function prototypes                                         ----*/
/*---------------------------------------------------------------------*/

PUB Boolean read_grammar(                  /* Out : can the grammar be read ?  */
FILE		*file,                     /* In  : the file to read from      */ 
Grammar_t  	*grammar,                  /* Out : the grammar to be read     */
Dictionary_t 	*dict,		    	   /* Out : the dictionary to be read  */
Symbol_list_t   *sl);

#endif /* parser_h */
