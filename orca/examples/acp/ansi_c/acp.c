/*=====================================================================*/
/*==== acp.c : active cahrt parser for recognizing                 ====*/
/*==           sentences                                             ==*/
/*==                                                                 ==*/

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "dictionary.h"
#include "types.h"
#include "parser.h"
#include "symbol_list.h"
#include "comb_list.h"
#include "nodelist.h"
#include "fifo.h"
#include "const.h"

/*
for Raoul's line profiler
#include "lmon.h"
*/

/*---------------------------------------------------------------------*/
/*---- typedefinitions                                                 */
/*---------------------------------------------------------------------*/

typedef enum {LEFT, RIGHT} Direction_t;     /* left or right neighbour */

/*---------------------------------------------------------------------*/
/*---- function prototypes                                             */
/*---------------------------------------------------------------------*/

PRIV Boolean add_new_pending(
	Edge_p          new_edge);

PRIV Boolean chart_parse(           
	Node_list_p     nl);           

PRIV Boolean check_stop_condition(
	Edge_p          edge,        
	int             nr_of_nodes,
	int             st_index);

PRIV Boolean combination_in_grammar(
	int             right1,  
	int        	right2,  
	Symbol_list_p 	sl);     


PRIV Boolean combine_neighbour(
        Direction_t     direction,
	Node_list_t     nl,        
	Edge_p          edge);     
        
PRIV Boolean combine_left(
	Node_list_t     nl,        
	Edge_p          edge);     

PRIV Boolean combine_right(
	Node_list_t     nl,        
	Edge_p          edge);     
  
PRIV void construct_edge(
	Edge_p		edge,        
	int             from_node,   
	int             to_node,     
	Edge_p          comb_edge_1, 
	Edge_p          comb_edge_2, 
	int             new_symbol); 

PRIV Boolean create_new_edges(
	Node_list_p     nl,            
	Edge_p          edge);         

PRIV Boolean do_creats(                    
	Grammar_p	gramm,     
	Dictionary_p	dict,      
	Job_queue_p	pending_edges);  

PRIV Boolean do_parse();

PRIV Boolean expand_symbol(
	Edge_p          edge);         


PRIV Boolean  handle_args(
        int             argc,
        char            **argv);

PRIV Boolean handle_word(
	Word_t          word,        
	int             node_nr);   

PRIV Boolean init_chart(
	Node_list_p     nl, 
	char            *line,
	int             nr_of_words);  

PRIV int nr_words(          
	char            *line);           

PRIV Boolean open_grammar_file( 
	FILE	**file);   

PRIV void print_dictionary(void);

PRIV void print_grammar(void);

PRIV void print_sentence(
        FILE            *f,
	Edge_t          edge,    
	int             level);  

PRIV Boolean read_sentence(           
	char		*line,           
	int		*nr_of_words);   


/*---------------------------------------------------------------------*/
/*---- global variables                                                */
/*---------------------------------------------------------------------*/

Dictionary_t Dict;			/* the dictionary */
Grammar_t    Gramm;			/* the production rules */
Symbol_list_t Symbols;                  /* list of symbols */
Job_queue_t  Pending_edges;		/* the pending edges table */
int          Nr_of_derivations = 0;	/* nr of possible derivations */
int          Verbose = 1;               /* print verbose messages? */

/*---------------------------------------------------------------------*/
/*---- functions                                                       */
/*---------------------------------------------------------------------*/


/*
 @ main() : acp's main program
 */
 
main(
     int     argc,
     char    **argv
)
{
    FILE *grammar_file;
    int  exit_status = 1;

/*
Raoul's line profiler
lmon_start(argv[0], NULL, 0, 0);
*/

    if (!handle_args(argc, argv))
        exit(0);
    
    if (!open_grammar_file(&grammar_file))
	exit(0);

    if (!do_creats(&Gramm, &Dict, &Pending_edges)){
	(void)fclose(grammar_file);
	exit(0);
    }
    
    v_print(fprintf(stdout, "Reading grammar....\n"));

    if (!read_grammar(grammar_file, &Gramm, &Dict, &Symbols)){
	fprintf(stderr, "ERROR : couldn't read grammar\n");
	exit_status = 0;
    }

    if (exit_status != 0){
	v_print(fprintf(stdout, "done!\n"));

	print_grammar();
	print_dictionary();

	
	if (!do_parse())
	    exit_status = 0;
    }
	
    v_print(fprintf(stdout, "deleting grammar..")); delete_grammar(&Gramm);
    v_print(fprintf(stdout, "done\ndeleting dictionary..."));delete_dict(&Dict);
    v_print(fprintf(stdout, "done\n"));

    (void)fclose(grammar_file);

    if (exit_status != 0)
	fprintf(stdout, "found a total of %d derivations\n", Nr_of_derivations);

/*
Raoul's line profiler
lmon_done();
*/

    return exit_status;
}


/*
 @  add_new_pending() : add a new edge to the pending edges table. Check for
 *                      double entries
 */

PRIV Boolean add_new_pending(
Edge_p		new_edge)		   /* In : edge to be added */
		
{
    if (!add_element(&Pending_edges, *new_edge)){
	fprintf(stderr, "can't add element to pending edges\n");
	return FALSE;
    }

    return TRUE;
}


/* 
 @ chart_parse : apply acp to parse sentence
 */

PRIV Boolean chart_parse(
Node_list_p	nl)                    	   /* In : the node list */
{
    Edge_p 	edge;                      /* edge from pending edges table */
    char        buffer[25];
    FILE        *f;  
    int         st_index;

    assert(is_in_sl(Symbols, STOP_SYMBOL, &st_index));

    while (nr_of_elts(Pending_edges) != 0){
	
	/* get element from pending edges queue */
	edge = delete_element(&Pending_edges);
	
	/* have we found a sentence ? */
	if (check_stop_condition(edge, nr_of_nodes(*nl), st_index)){
/*
            if ((Nr_of_derivations + 1)%100 == 0){
		fprintf(stdout, ">>>>>>>>>>>>>>>>Found sentence %d!!!!!!!<<<<<<<<<<<<<<<<<<<\n", Nr_of_derivations + 1);
	    }
*/
	    
	    Nr_of_derivations++;

/*
            sprintf(buffer, "./tmp/deriv.%d", Nr_of_derivations);
            if ((f = fopen(buffer, "w+")) == NULL){
                perror(buffer);
                exit(0);
            }      
*/

	    v_print(print_sentence(stdout, *edge, 1));

/*
            if (fclose(f) == EOF){
                perror("fclose");
                exit(0);
            }

*/

	}
	/* add edge to chart */

        if (!add_outgoing(edge->from_node, edge, nl)){
            fprintf(stderr, "ERROR, can't add outgoing to nodelist\n");
	    return FALSE;
        }

        if (!add_incoming(edge->to_node, edge, nl)){
            fprintf(stderr, "ERROR, can't add incoming to nodelist\n");
	    return FALSE;
        }

	/* add new edges that can be made by last addition */
	if (!create_new_edges(nl, edge)){
	    return FALSE;
	}

    }
    return TRUE;
}


    
/*
 @ check_stop_condition() : have we found the symbol we're looking for? 
 */    

PRIV Boolean check_stop_condition(
Edge_p 		edge,                      /* In : the edge */
int		nr_of_nodes,               /* In : nr of nodes */
int             st_index)
{
    return ((edge->from_node == 1) && 
            (edge->to_node == nr_of_nodes) &&
	    (edge->symbol_name == st_index));

/*	    (strncmp(STOP_SYMBOL , edge->symbol_name, MAX_SYMBOL_LENGTH) == 0));
*/
}


/*
 @ combination_in_grammar() : check if a combination of non-terminals is in the grammar
 */
	
PRIV Boolean combination_in_grammar(
int      	right1,                    /* In : first right hand side symbol */
int             right2,                    /* In : second right hand side symbol */
Symbol_list_p   sl)                        /* Out: list of left hand side symbols */
{
    int         gramm_index, cl_index;     /* indices in lists */
    Comb_list_t cl;                        /* combination list */
    Symbol_name_t r1, r2;
    Symbol_name_t empty  = "";

    /* check if rule " -> right1 right2 " is in the grammar */

    assert(give_ith_sl_elt(Symbols, right1, r1));
    if (right2 == 0) 
	strncpy(r2, empty, MAX_WORD_LENGTH);
    else
	assert(give_ith_sl_elt(Symbols, right2, r2));

    if (is_in_grammar(Gramm, r1, &gramm_index)){
	if (!give_rule_info(Gramm, gramm_index, r1, &cl)){
	    fprintf(stderr, "ERROR: Give_rule_info()\n");
	    exit(0);
	}

	if (is_in_comb_list(cl, r2, &cl_index)){
	    if (!give_comb_info(cl, cl_index, r2, sl)){
		fprintf(stderr, "ERROR: Give_comb_info()\n");
		exit(0);
	    }
	    return TRUE;
	}
    }
    return FALSE;
}


/* 
 @ combine_neighbour() :  combine an edge in the chart with its {left, right} neighbour
 *                        edges
 */

PRIV Boolean combine_neighbour(
Direction_t     dir,                       /* In : combine wirth left or right 
					      neighbour ? */ 
Node_list_t     nl,                  	   /* In : the list of nodes  */
Edge_p          edge)                      /* In : the edge under consideration */
{
    int         j, k;		           /* loop indices */
    int         num_nb_edges;              /* nr of neighbouring edges */
    Node_t      node;			   /* src node of edge */
    Edge_t      new_edge;		   /* new edge to add */
    Edge_p      neighbour_edge;  	   /* neighbour edge */
    Edge_p      l_edge, r_edge;            /* left and right edges of a combination */
    Symbol_list_t  sl;                     /* list of symbols (i.e. non-terminals) that 
					    * can be created by combination of edges */
    Symbol_name_t  new_symbol;             /* name of new symbol */
    int            index; 


    /* get info on {left, right} neighbour of edge */
    if (!get_node_info(nl, (dir==LEFT)?(edge->from_node):(edge->to_node), &node)){
	fprintf(stderr, "ERROR: Get_node_info()\n");
	return FALSE;
    }

    /* Try all combinations of current edge and {incoming, outgoing} edges of 
     * from_node 
     */

    num_nb_edges = nr_of_edges((dir==LEFT)?(node.incoming_edges):(node.outgoing_edges));
    for(j = 1; j <= num_nb_edges; j++){
	if (!get_edge_info((dir==LEFT)?(node.incoming_edges):(node.outgoing_edges), 
			   j, &neighbour_edge)){
	    fprintf(stderr, "ERROR: Get_edge_info()\n");
	    return FALSE;
	}
	
	l_edge = (dir==LEFT)?(neighbour_edge):(edge);
	r_edge = (dir==LEFT)?(edge):(neighbour_edge);

	if (combination_in_grammar(l_edge->symbol_name, r_edge->symbol_name, 
				   &sl)){
	    /* Add all new combinations to the chart */
	    for (k = 1; k <= nr_of_sl_elts(sl); k++){
		if (!give_ith_sl_elt(sl, k, new_symbol)){
		    fprintf(stderr, "ERROR: Give_ith_sl_elt\n");
		    return FALSE;
		}

		assert(is_in_sl(Symbols, new_symbol, &index));
		construct_edge(&new_edge, l_edge->from_node, r_edge->to_node, 
			       l_edge, r_edge, index);
		if (!add_new_pending(&new_edge))
		    return FALSE;
	    }
	}
    }
    
    return TRUE;
}



/*
 @ construct_edge() : fill in edge structure
 */

PRIV void construct_edge(
Edge_p 		edge,			   /* Out : edge to be filled */
int       	from_node, 		   /* In  : from_node of  edge */
int       	to_node, 		   /* In  : to_node of edge    */
Edge_p    	comb_edge_1, 		   /* In  : ptr to first combination edge */
Edge_p    	comb_edge_2,               /* In  : ptr to second combination edge */
int             new_symbol)		   /* In  : label of new edge */
{
/*    Word_t empty = "";
*/

    edge->from_node = from_node;
    edge->to_node   = to_node;
    edge->comp_1 = comb_edge_1;
    edge->comp_2 = comb_edge_2;
/*
    strncpy(edge->symbol_name, new_symbol, MAX_SYMBOL_LENGTH);
*/
    edge->symbol_name = new_symbol;

    edge->terminal = 0;

/*    strncpy(edge->terminal, empty, MAX_WORD_LENGTH);
*/
}


/* 
 @  create_new_edges() : add new edges to chart by combining newly added
 *                       edge with its neighbours in the chart
 */

PRIV Boolean create_new_edges(
Node_list_p  nl,                       	   /* In : the node list */
Edge_p       edge)                         /* In : the edge with which to combine */
					   
{
    /* first of all, see if the edge symbol can be expanded by its
     * own, without combinations, e.g. in the rule:
     *                 noun_clause   -> noun
     */

    if (!expand_symbol(edge)) return FALSE;

    /* combine edge with left neighbours */
    if (!combine_neighbour(LEFT, *nl, edge))
	return FALSE;

    /* combine edge with right neighbours */
    if (!combine_neighbour(RIGHT, *nl, edge))
	return FALSE;

    return TRUE;
}


/*
 @ do_creats() : create global variables 
 */

PRIV Boolean do_creats(                    
Grammar_p  	gramm,             	   /* Out : grammar            */
Dictionary_p    dict,               	   /* Out : dictionary         */
Job_queue_p     pending_edges)             /* Out : pending edges list */
{
    v_print(fprintf(stdout, "done\nCreating dictionary..."));
    if (!create_dict(dict)){
	fprintf(stderr, "ERROR: couldn't create dictionary\n");
	return FALSE;
    }
    
    v_print(fprintf(stdout, "done\nCreating grammar..."));
    if (!create_grammar(gramm)){
	fprintf(stderr, "ERROR: couldn't create grammar\n");
	delete_dict(dict);
	return FALSE;
    }
    
    v_print(fprintf(stdout, "done\n Creating pending edges..."));
    if (!create_job_q(pending_edges)){
	fprintf(stderr, "ERROR: couldn't create pending edge queue\n");
	delete_dict(dict);
	delete_grammar(gramm);
	return FALSE;
    }

    v_print(fprintf(stdout, "done\n"));
    return TRUE;
}


/*
 @ do_parse() : setup and do parsing of sentence
 */ 

PRIV Boolean do_parse()
{
    int		nr_of_words;

    Node_list_t nl;			   /* node list */
    Line_t      line;			   /* line input by the user */
    Boolean     bool = TRUE;		   /* Everything ok so far? */

    if (!read_sentence(line, &nr_of_words))
	return FALSE;

    v_print(fprintf(stdout, "creating node_list..."));

    /* #nodes is 1 larger than #words */
    if (!create_node_list(nr_of_words + 1, &nl)){
	fprintf(stderr, "ERROR: failed creating node list\n");
	return FALSE;
    }
    v_print(fprintf(stdout, "done\n"));

    bool = init_chart(&nl, line, nr_of_words);
	
    if (bool){
	v_print(fprintf(stdout, "Searching for derivations......\n"));
	bool = chart_parse(&nl);
    }

    v_print(fprintf(stdout, "deleting nl..")); 
    delete_node_list(&nl);
    v_print(fprintf(stdout, "done\n"));

    return bool;
}


/* 
 @ expand_symbol() : see if edge symbol can be expanded without need of
 *		     a combination with neighbour edge (i.e. combination
 *		     with "empty" neighbour
 */

PRIV Boolean expand_symbol(
Edge_p		edge)			   /* In : edge under consideration */
{
    Edge_t         new_edge;		   /* new edge */
    Symbol_name_t  new_symbol;             /* label of new edge */
    Symbol_name_t  empty = "";             /* dummy edge label */
    Symbol_list_t  sl;			   /* list of symbols (i.e. non-terminals) that
                                            * can be created by expansion of edge */
    int            k;			   /* loop index */
    int            index;

    /* if rule "new_symbol -> edge.symbol_name" is in the gramar,
       then add new_symbol to the pending edges table.
     */

    if (combination_in_grammar(edge->symbol_name, 0, &sl)){
	/* Add all new combinations to the chart */
	for (k = 1; k <= nr_of_sl_elts(sl); k++){
	    if (!give_ith_sl_elt(sl, k, new_symbol)){
		fprintf(stderr, "ERROR: Give_ith_sl_elt\n");
		return FALSE;
	    }

	    assert(is_in_sl(Symbols, new_symbol, &index));
	    construct_edge(&new_edge, edge->from_node, edge->to_node,
			   edge, (Edge_p)NULL, index);
	    if (!add_new_pending(&new_edge))
		return FALSE;
	}
    }
    
    return TRUE;
}



 /* 
  @ handle_args() : handle commandline arguments
  */

Boolean handle_args(
int             argc,                      /* nr of arguments */
char            **argv)                    /* argument vector */
{
    extern int optind;
    extern char *optarg;
    int c, errflag;

    errflag = 0;
    Verbose = 1;
    while((c = getopt(argc, argv, "v")) != -1){
        switch(c){
          case 'v' : fprintf(stdout,"verbose mode is off\n");
                     Verbose = 0;
                     break;
          default  : errflag++;
        }
    }

    if ((optind != argc) || (errflag)){
        fprintf(stdout, "usage: %s [-v]\n", argv[0]);
        fprintf(stdout, "-v disables verbose messages\n");
        fprintf(stdout, "%d %d\n", optind, errflag);
        return FALSE;
    }
    
    return TRUE;
}


/*
 @ handle_word() : check if a word is in the dictionary and if so, put
 *                 it in the chart 
 */

PRIV Boolean handle_word(
Word_t		word,			   /* In : the word to be checked/added */
int 		node_nr)		   /* In : left node number of word */
{
    int           i;    		   /* loop index */
    int           index;                   /* index of word in dictionary */    
    Edge_t        edge;                    /* edge representing the word */
    Symbol_list_t sl;			   /* non-terminals that expand to word */
    Symbol_name_t symbol;                  /* symbol name of edge */
    
    /* is the word element of the dictionary */

    if (!is_in_dict(Dict, word, &index)){
	fprintf(stderr, "ERROR:     <<<<<<%s>>>>>>> not in grammar\n", word);
	return FALSE;
    }
    
    if (!give_word_info(Dict, index, word, &sl)){
	fprintf(stderr, "ERROR: Give_word_info()\n");
	return FALSE;
    }

    for (i = 1; i <= nr_of_sl_elts(sl); i++){
        if (!give_ith_sl_elt(sl, i, symbol)){
            fprintf(stderr, "ERROR: Give_ith_sl_elt\n");
            return FALSE;
        }
        v_print(fprintf(stdout, "    found: %10s -> %10s\n", symbol, word));

	edge.comp_1 = (Edge_p)NULL;
	edge.comp_2 = (Edge_p)NULL;
	edge.from_node = node_nr;
	edge.to_node = node_nr + 1;
	assert(is_in_sl(Symbols, symbol, &(edge.symbol_name)));

/*	strncpy(edge.symbol_name, symbol, MAX_SYMBOL_LENGTH);
	strncpy(edge.terminal, word, MAX_WORD_LENGTH);
*/
	edge.terminal = index;

	v_print(fprintf(stdout, "adding edge to pending edges table..."));
	if (!add_element(&Pending_edges, edge)){
	    fprintf(stderr, "ERROR: can't add edge to pending edges table\n");
	    return FALSE;
	}
	v_print(fprintf(stdout, "done\n"));
    }
    return TRUE;
}    


/*
 @ init_chart() : initizlize the chart
 */

PRIV Boolean init_chart(
Node_list_p     nl,                        /* Out : node list      */
char            *line,                     /* In  : sentence to be parsed */
int             nr_of_words)               /* In  : nr of words in the line */
{
    int         node_nr = 1;               /* node number in the chart */
    Word_t      word;			   /* word on input */
    char        *tok;			   /* pointer in line */

    v_print(fprintf(stdout, "Dissecting line...\n"));
    
    if ((tok = strtok(line, " \n")) == NULL){
	fprintf(stderr, "ERROR dissecting line\n");
	return FALSE;
    }

    strncpy(word, tok, MAX_WORD_LENGTH);
    v_print(fprintf(stdout, "read: <%s>\n", word));
    if (!handle_word(word, node_nr))
	return FALSE;

    while ((tok = strtok(NULL, " \n")) != NULL){
	node_nr++;
	strncpy(word, tok, MAX_WORD_LENGTH);
	v_print(fprintf(stdout, "read <%s>\n", word));
	if (!handle_word(word, node_nr))
	    return FALSE;
    }
    
    return TRUE;
}


/*
 @    return nr of words in a line
 */

PRIV int nr_words(                         /* Out : nr of words in a line */
char		*line)                     /* In  : the line of characters */
{
    int 	cnt;			   /* nr of spaces */ 
    char 	*ch;                       /* ptr to character in the line */

    cnt = 0;
 
    ch = line;
    while (*ch != '\n'){
	/* if char is a blank, skip all blanks following it */
	if (*ch == ' '){
	    *ch++;
	    while (*ch == (int)' '){
		*ch++;
	    }
	    *ch--;
	    cnt++;
	}
	*ch++;
    }
    return cnt + 1;   /* nr of words is #spaces + 1 */
}


/* 
 @ open_grammar_file() : open the file from which the grammar is to be read 
 */

PRIV Boolean open_grammar_file(
FILE    **file                              /* Out : input file */
)
{

    v_print(fprintf(stdout, "opening grammar file .."));
    if ((*file = fopen(GRAMMAR_FILE, "r")) == NULL){
	perror(GRAMMAR_FILE);
	return FALSE;
    }
    return TRUE;
}


/* 
 @ print_dictionary() : print the dictionary of words
 */

PRIV void print_dictionary()
{
    int           i, j;			   /* loop indices */
    Word_t        word;			   /* word in the dictionary */
    Symbol_name_t symbol;		   /* non-terminal symbol */
    Symbol_list_t sl;			   /* symbol list */

    v_print(fprintf(stdout, "\n==========================Dictionary======================\n"));
    v_print(fprintf(stdout, " nr of words in grammar is %d\n", nr_of_dict_words(Dict)));

    for (i=1; i<= nr_of_dict_words(Dict); i++){
	
	if (!give_word_info(Dict, i, word, &sl)){
	    fprintf(stderr, "ERROR: Give_word_info\n");
	    exit(0);
	}

	for (j = 1; j <= nr_of_sl_elts(sl); j++){
	    if (!give_ith_sl_elt(sl, j, symbol)){
		fprintf(stderr, "ERROR: Give_ith_sl_elt\n");
		exit(0);
	    }
	    v_print(fprintf(stdout, "%20s -> %20s\n", symbol, word));
	}
    }
}


/* 
 @ print_grammar() : print the grammar read
 */

PRIV void print_grammar()
{
    int            i, j, k;		   /* loop indices */
    Comb_list_t    cl;			   /* combination list */
    Symbol_name_t  right1, right2, left;   /* lhs, and 2 rhs of a rule */
    Symbol_list_t  sl;			   /* symbol list */

    v_print(fprintf(stdout, "\n==========================Grammar======================\n"));
    v_print(fprintf(stdout, " nr of rules in the grammar is %d\n", nr_of_rules(Gramm)));
    
    for(i=1 ;i <= nr_of_rules(Gramm); i++){
	if (!give_rule_info(Gramm, i, right1, &cl)){
	    fprintf(stderr, "ERROR: Give_rule_info()\n");
	    exit(0);
	}
	
	for (j = 1; j <= nr_of_combinations(cl); j++){
	    if (!give_comb_info(cl, j, right2, &sl)){
		fprintf(stderr, "ERROR: Give_comb_info()\n");
		exit(0);
	    }

            for (k = 1; k <= nr_of_sl_elts(sl); k++){
                if (!give_ith_sl_elt(sl, k, left)){
                    fprintf(stderr, "ERROR: Give_ith_sl_elt\n");
		    exit(0);
                }
		
		v_print(fprintf(stdout, "%20s -> %20s %20s\n", left, right1, right2));
	    }
	}
    }
}


/*
 @ print_sentence() : print parse tree of a sentence 
 */

PRIV void print_sentence(
FILE            *f,
Edge_t       	edge,                      /* In : the sentence edge */
int          	level)			   /* In : level of indentation */
		
{
    int 	i;			   /* loop var */
    Symbol_name_t  empty = "";             /* dummy edge label */
    Symbol_name_t right1, right2;
    Symbol_list_t sl;
    int index;
    Symbol_name_t symbol;
    Word_t       terminal;

    if ((edge.comp_1 == NULL) && (edge.comp_2 == NULL)){
	/* lowest level */

        assert(give_word_info(Dict, edge.terminal, terminal, &sl));

/*
        if (!is_in_dict(Dict, terminal, &index)){
            fprintf(stderr, "ERROR:     <<<<<<%s>>>>>>> not in grammar\n", 
		    terminal);
            exit(0);
        }

        assert(is_in_sl(sl, edge.symbol_name, &index));
 */
	for (i=1; i <= level*2; i++)
	    fprintf(f, " ");
	
	assert(give_ith_sl_elt(Symbols, edge.symbol_name, symbol));

	fprintf(f, "[%s %s]\n", symbol, terminal);
	return;
    }

/*      
    strncpy(right1, edge.comp_1->symbol_name, MAX_SYMBOL_LENGTH);
    if (edge.comp_2 != NULL)
        strncpy(right2, edge.comp_2->symbol_name, MAX_SYMBOL_LENGTH);
    else
        strncpy(right2, empty, MAX_SYMBOL_LENGTH);
    
    if (combination_in_grammar(right1, right2, &sl)){
        if (is_in_sl(sl, edge.symbol_name, &index)){
            assert(give_ith_sl_elt(sl, index, symbol));

            for (i=1; i <= level*2; i++)
                fprintf(f, " ");
            fprintf(f, "found : %s -> %s %s\n", symbol, right1, right2);
        }else{
            fprintf(stderr, "major FU. %s -> %s %s is NOT in grammar\n", symbol, right1, right2)
;
            exit(0);
        }
    }else{
        fprintf(stderr, "major FU. combination %s %s NOT in grammar\n", right1, right2);
        exit(0);
    }
*/

    for (i=1; i <= level*2; i++)
	fprintf(f, " ");
    
    assert(give_ith_sl_elt(Symbols, edge.symbol_name, symbol));

    fprintf(f, "[%s\n", symbol);
    
    /* print left edge */
    if (edge.comp_1 != NULL)
	print_sentence(f, *(edge.comp_1), level + 1);
    
    /* print right edge */
    if (edge.comp_2 != NULL)
	print_sentence(f, *(edge.comp_2), level + 1);
    
    for (i=1; i <= level*2; i++)
	fprintf(f,  " ");
    fprintf(f, "]\n");
}


/*
 @ read_sentence() : read sentence from standard input
 */

PRIV Boolean read_sentence(
char	       	*line,			   /* Out : line input by the user */
int		*nr_of_words)		   /* Out : nr of words on line */
{
    fprintf(stdout, "give sentence : ");
    if (fgets(line, LINE_LEN - 1, stdin) == NULL){
	fprintf(stderr, "ERROR : can't read sentence\n");
	return FALSE;
    }
    
    fprintf(stdout, "read %s\n", line);

    *nr_of_words = nr_words(line);
    fprintf(stdout, "total words in sentence was: %d\n", *nr_of_words);

    return TRUE;
}


