/*=====================================================================*/
/*==== types.h : type definitions                                  ====*/
/*==                                                                 ==*/

#ifndef types_h
#define types_h

#include "const.h"
#include "malloc.h"

#define v_print(x)   if (Verbose) x

typedef unsigned short Boolean;

typedef char Line_t[LINE_LEN];                 /* a line on the input file */

typedef char Symbol_name_t[MAX_SYMBOL_LENGTH]; /* grammar symbol name */

typedef char Word_t[MAX_WORD_LENGTH];          /* a word in a sentence */


typedef struct {                               /* representation the rule */
    Symbol_name_t new,right1,                  /*     new <- right1 right2 */
                  right2;
} Rule_t;



#endif /* types_h */
