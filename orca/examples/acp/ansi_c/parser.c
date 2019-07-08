/*=====================================================================*/
/*==== parser.c : parser function implementations                  ====*/
/*==                                                                 ==*/

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "types.h"
#include "grammar.h"
#include "const.h"
#include "parser.h"

/*---------------------------------------------------------------------*/
/*---- constant definitions                                            */
/*---------------------------------------------------------------------*/

#define SPACE           ((int)' ')
#define TAB             ((int)'\t')
#define EOLN            ((int)'\n') 

/*---------------------------------------------------------------------*/
/*---- global variables                                                */
/*---------------------------------------------------------------------*/

static  int    Line_no = 0 ;            /* line number used for eror messages */


/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PRIV Boolean add_rule(
Rule_t        	rule,      
Grammar_t     	*grammar,
Symbol_list_t   *sl);

PRIV Boolean add_terminal(
Rule_t        	rule, 
Dictionary_t  	*dictionary,
Symbol_list_t   *sl); 

PRIV int next_character(   
FILE       	*file);  
		
PRIV int nextchar(      
FILE      	*file);

PRIV void skip_comment(
FILE       	*file); 

PRIV void skip_line(
FILE        	*file);

PRIV Boolean skip_spaces(
FILE 		*file);  

PRIV void print_char(
int         	c);	

PRIV Boolean read_character(
FILE        	*file,  
int         	expected);

PRIV Boolean read_dict(         
FILE         	*file,       
Dictionary_t 	*dictionary,
Symbol_list_t   *sl);

PRIV Boolean read_prod_rules(
FILE       	*file,      
Grammar_t  	*grammar,
Symbol_list_t   *sl); 

PRIV Boolean read_rule(      
Line_t          line,       
Rule_t     	*rule);    

PRIV Boolean read_symbol(
FILE       	*file,  
Symbol_name_t   *symbol);



/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/


/*
 @ add_rule() : add production rule to the grammar and the symbol list
 */

PRIV Boolean add_rule(			   /* Out: succes or not */
Rule_t        	rule,                      /* In : rule to be added */
Grammar_t     	*grammar,                  /* Out : the updated grammar */
Symbol_list_t   *sl)
{
    fprintf(stdout, "adding rule %s <- %s %s\n", rule.new, rule.right1, rule.right2);
    if (!insert_prod_rule(rule, grammar)){
        fprintf(stderr, ">>>>Couldn't add to gramar\n");
        return FALSE;
    }else{
        fprintf(stdout, "added succesfully\n");
    }

    if (!(insert_sl_elt(rule.new, sl) &&
	  insert_sl_elt(rule.right1, sl) &&
	  insert_sl_elt(rule.right2, sl))){
	fprintf(stderr, ">>>>Couldn't add to symbollist\n");
	return FALSE;
    }

    return TRUE;
}


/*
 @ add_terminal() :  add word to dictionary 
 */

PRIV Boolean add_terminal(		   /* Out: succes or not */
Rule_t        	rule,                      /* In : rule to be added */
Dictionary_t  	*dictionary,               /* Out : the updated dictionary */
Symbol_list_t   *sl)
{
    fprintf(stdout, "adding rule <%s <- %s>   ....", rule.new, rule.right1); 
    if (!insert_dict_word(rule.right1, rule.new, dictionary)){
	printf(">>>>Couldn't add to dictionary\n");
	return FALSE;
    } else{
	printf("added succesfully\n");
    }
    
    assert(insert_sl_elt(rule.right1, sl));

    return TRUE;
}

/*
 @ next_character() : return next non-blank character 
 */

PRIV int next_character(		   /* Out: the next non-blank character    */
FILE	       	*file)                     /* In: The file to read from  */
{
    int		c;                         /* character read from input  */

    /* skip whitespaces */
    (void)skip_spaces(file);

    /* get char from input and put it back */
    c = getc(file);
    ungetc(c, file);
    return c;
}       


/*
 @ nextchar() : return next charcter from input 
 */

PRIV int nextchar(			   /* Out: the next character */
FILE		*file)                     /* In: The file to read from */
{
    int    	c;                         /* character read from input */

    /* get char from input and put it back */
    c = getc(file);
    ungetc(c, file);
    return c;
}       

/*
 @ print_char() : print character. Take special care of <eoln> and <eof>
 */

PRIV void print_char(
int         	c)                         /* In: the character to print */ 
{
    if (c == EOLN)
        fprintf(stderr, "<eoln>");
    else if (c == EOF)
        fprintf(stderr, "<eof>");
    else
        fprintf(stderr, "%c", (char)c); 
}


/* 
 @ read_character() : skip blanks, read character and check if it is 
 *                    the character expected
 */

PRIV Boolean read_character(             /* Out: succesful of not      */
FILE        	*file,                   /* In: The file to read from  */
int             expected)                /* In: The character expected */
{
    int     c;                           /* character read from input  */

    /* first skip whitespaces */
    (void)skip_spaces(file);

    /* if next char on input does not equal the char expected,
     * print an errormessage 
     */

    if ((c = getc(file)) != expected){
        fprintf(stderr, "Error in config file, ");
        print_char(expected);
        fprintf(stderr, " expected instead of '");
        print_char(c);
        fprintf(stderr, "'\n");
        return FALSE;
    }

    return TRUE;
}


/*
 @ read_dict() : read dictionary 
 */

PRIV Boolean read_dict(			   /* Out : can the dictionary be read ? */
FILE         	*file,                     /* In  : the file to read from      */
Dictionary_t 	*dictionary, 
Symbol_list_t   *sl)
{
    /* Read the dictionary, i.e. all terminals in the grammar .
       Expected input format: {<symbol> <spaces>  '<' '-' <spaces> <terminal>}
    */

    Line_t  	line;                      /* The line from the input file */
    Rule_t  	rule;			   /* production rule */

    while (next_character(file) != EOF){

	/* does the line start with a comment? */
	if (nextchar(file) == COMMENT){
	    skip_comment(file);
	    continue;
	}
	
	Line_no++;
	/* read the line */
	if (fgets(line, LINE_LEN - 1, file) == NULL){
	    perror("fgets");
	    return FALSE;
	}
	   
	/* extract the rule */
	if (read_rule(line, &rule)){
	    if (!add_terminal(rule, dictionary, sl)){
		fprintf(stderr, " can't add rule");
		return FALSE;
	    }
	}else{
	    fprintf(stderr, "Grammar file <%s> error in line %d\n", GRAMMAR_FILE, Line_no);
	    return FALSE;
	}
    }

    return TRUE;
}    


/* 
 @ read_grammar() : read production rules and dictionary
 */

PUB Boolean read_grammar(		   /* Out : can the grammar be read ?  */
FILE       	*file,                     /* In  : the file to read from      */ 
Grammar_t  	*grammar,                  /* Out : the grammar to be read     */
Dictionary_t 	*dict, 			   /* Out : the dictionary to be read  */
Symbol_list_t   *sl)
{
    /* Read the grammar. Expected input format:
	<production rules> <eoln> <dictionary> <eof>
     */
    
    return   read_prod_rules(file, grammar, sl) &&
             read_character(file, EOLN) &&
             read_dict(file, dict, sl);
}


/*
 @ read_prod_rules() :  read all production rules of the grammar
 */

PRIV Boolean read_prod_rules(		   /* Out : can the grammar be read ? */
FILE       	*file,                     /* In  : the file to read from     */ 
Grammar_t  	*grammar,                  /* Out : the grammar to be read    */
Symbol_list_t   *sl)
{
    Rule_t 	rule;			   /* a production rule */
    Line_t 	line;			   /* input line */

    /* while not end of the grammar part */
    while (next_character(file) != EOLN){
	
	/* does the line start with a comment? */
	if (nextchar(file) == COMMENT){
	    skip_comment(file);
	    continue;
	}

	/* read the line containing the production rule */
	Line_no ++;
	if (fgets(line, LINE_LEN - 1, file) == NULL){
	    perror("fgets");
	    return FALSE;
	}
	   
	/* extract the rule from it */
	if (read_rule(line, &rule)){
	    if (!add_rule(rule, grammar, sl)){
		fprintf(stderr, " can't add rule");
		return FALSE;
	    }
	}else{
	    fprintf(stderr, "Grammar file <%s> error in line %d\n", GRAMMAR_FILE, Line_no);
	    return FALSE;
	}
    }

    return TRUE;
}

    
/*
 @ read_rule : try to read production rule of the grammar
 */

PRIV Boolean read_rule(			   /* Out : can the rule be read ? */
Line_t		line,                      /* In  : the line to read from */
Rule_t    	*rule)                     /* Out : the production rule */
{
    /* read production rule. Expected input format: 
       <symbol> <spaces> '->' <spaces> <symbol> [<symbol>]
     */

    char   	*tok;                      /* pointer to next token in the line*/

    /* get left hand side */
    if ((tok = strtok(line, " ->")) == (char *)NULL)
	return FALSE;
    (void)strcpy(rule->new, tok);
   
    /* get first symbol of righthand side */
    if ((tok = strtok(NULL, " ->\n")) == (char *)NULL)
	return FALSE;
    (void)strncpy(rule->right1, tok, MAX_SYMBOL_LENGTH);

    /* check if there is a second righthand term */
    if ((tok = strtok(NULL, "\n")) != (char *)NULL){
	/* yes there is */
	(void)strncpy(rule->right2, tok, MAX_SYMBOL_LENGTH);
    } else
	strncpy(rule->right2, "", MAX_SYMBOL_LENGTH);
    
    return TRUE;
}


/*
 @ read_symbol() : read non-terminal symbol from input
 */

PRIV Boolean read_symbol(                  /* Out : can the symbol be read ? */
FILE		*file,                     /* In  : the file to read from */
Symbol_name_t   *symbol)                   /* Out : the symbol read */
{

    int 	length = 0;		   /* length of symbol */
    int 	ch;			   /* character read from input */

    while ((nextchar(file) != SPACE) && (nextchar(file) != EOLN)){
	ch = getc(file);
	if (length == MAX_SYMBOL_LENGTH-1){
	    fprintf(stderr, "Error: symbolname <%s> too long. Max = %d\n", symbol, 
		    MAX_SYMBOL_LENGTH);
	    return FALSE;
	}
	(*symbol)[length++] = (char)ch;
    }

    if (length < MAX_SYMBOL_LENGTH)
	(*symbol)[length] = '\0';

    return TRUE;
}
	

/* 
 @ skip_comment() : skip line containing comment sign at the beginning of the line
 */

PRIV void skip_comment(
FILE		*file)                    /* In: The file to read from         */
{
    /* while the line begins with a COMMENT sign, skip the line */

    while (nextchar(file) == COMMENT){
        skip_line(file);
    }
}


/*
 @ skip_line() : skip crest of the line, including newline
 */

PRIV void skip_line(
FILE 	       *file)                     /* In: The file to read from */
{
    int     c;                            /* character read from input */
        
    /* while not eoln seen, skip characters. 
     */

    c = getc(file);
    while ((c != EOLN) && (c != EOF)){
        c = getc(file);
    }
}


/*
 @ skip_spaces() : skip blanks from input
 */

PRIV Boolean skip_spaces(
FILE 		*file)                     /* In: The file to read from */
{
    int		c;			   /* character from input */

    /* while next character is whitespace, skip it */
    c = getc(file);
    while ((c == SPACE ) || (c == TAB))
        c = getc(file);
    ungetc(c, file);
    
    return TRUE;     /* always return TRUE */
}


