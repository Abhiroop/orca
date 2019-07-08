/*=====================================================================*/
/*==== const.h : global constant definitions                       ====*/
/*==                                                                 ==*/

#ifndef const_h
#define const_h

/* definition of global constants */

#define TRUE              1
#define FALSE             0

#define MAX_SYMBOL_LENGTH 15              /* max length of a symbol in the grammar */
#define MAX_WORD_LENGTH   15              /* max length for a word in a sentence   */

#define GRAMMAR_FILE      "grammar"       /* the file that contains the grammar */
#define STOP_SYMBOL       "sentence"      /* symbol to check for */
#define LINE_LEN          256             /* maximum input line length */

#define COMMENT           ((int)'#')      /* comment sign in gramar file */

#define PRIV              static
#define PUB               /**/

#endif /* const_h */

