#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pan_trace.h"

#include "states.h"



/*-- Types -------------------------------------------------------------------*/


/*-- Static globals ----------------------------------------------------------*/



#define MAX_EVTYPES 100         /* initial max no. of event types */


/** GLOBAL ARRAYS AND POINTERS --------------------------------------------- **/

static state_p     states = NULL;	/* beginning of state queue */
static state_p     available = NULL;	/* queue of empty, ready to use nodes */

static size_t      n_states = 0;

static state_p    *state_ref = NULL;	/* event belongs to which state? */
static size_t      n_state_ref = 0;



/*---- Utility routines -----------------------------------------------------*/


state_p
first_state(void)
{
    return states;
}



state_p
next_state(state_p prev_state)
{
    return prev_state->next;
}



int
num_states(void)
{
    return n_states;
}



state_p
state_locate(int event)
{
    if (event >= n_state_ref) {
	return NULL;
    } else {
	return state_ref[event];
    }
}



void
start_state_module(void)
{
    int         i;

    n_states  = 0;
    n_state_ref = MAX_EVTYPES;
    state_ref   = pan_malloc(n_state_ref * sizeof(state_p));
    for (i = 0; i < n_state_ref; i++)
	state_ref[i] = NULL;

    available = NULL;
}



/****************************************************************************/
/* GET STATE NODE: is a function that manages the creation of state nodes by*/
/*                 maintaining a queue of available, unsused nodes.  Every  */
/*                 time a state-node is requested it will check the queue   */
/*                 for an available node first and return a pointer to it,  */
/*                 or create a new one in case the queue is empty.          */
/*                 In addition it will fill the node in with the given data */
/*                 so it is ready to use upon returning...                  */
/****************************************************************************/
static state_p
GetStateNode(int start, int end, char color[], char name[], int pattern)
{
    state_p nextNode;

    if (available == NULL) {		/* queue is empty */
	nextNode = pan_malloc(sizeof(state_t));
    } else {
	nextNode = available;
	available = nextNode->next;

    }
					/* fill the node with given data */
    nextNode->start      = start;
    nextNode->end        = end;
    nextNode->name       = strdup(name);
    nextNode->color_name = strdup(color);
    nextNode->pattern    = pattern;

    nextNode->next       = NULL;

    ++n_states;

    return nextNode;

}



static void
resize_array(size_t entry, size_t *size, void *array[], size_t elt_size)
{
    int         old_size;
    int         new_size = *size;

    old_size = new_size;
    while (entry >= new_size) {
	new_size *= 2;
    }
    if (old_size != new_size) {
	*array = pan_realloc(*array, new_size * elt_size);
	*size  = new_size;
	memset(((char *)(*array)) + old_size * elt_size, '\0',
	       (new_size - old_size) * elt_size);
    }
}


static int
fscan_upto(FILE *fp, char str[], char sentinel)
{
    char c;

    while (TRUE) {
	c = getc(fp);
	if (c == EOF) {
	    return EOF;
	} else if (c == sentinel) {
	    *str = '\0';
	    fscanf(fp, " ");
	    return 1;
	} else if (c == '\\') {
	    c = getc(fp);
	    if (c == '\n') {
		*str = ' ';
		++str;
	    } else if (c == sentinel) {
		*str = c;
		++str;
	    } else {
		*str = '\\';
		++str;
		*str = c;
		++str;
	    }
	} else {
	    *str = c;
	    ++str;
	}
    }
}


static int
fscan_string(FILE *fp, char str[])
{
    char c;

    c = getc(fp);
    if (c == '"') {
	return fscan_upto(fp, str, '"');
    } else if (c == '\'') {
	return fscan_upto(fp, str, '\'');
    } else {
	*str = c;
	++str;
	return fscanf(fp, "%s ", str);
    }
}


static int
read_state(FILE *fp, int *id, char start_name[], char end_name[], char color[],
	   char name[])
{
    if (fscanf(fp, "%d ", id)        == EOF) return EOF;
    if (fscan_string(fp, start_name) == EOF) return EOF;
    if (fscan_string(fp, end_name)   == EOF) return EOF;
    if (fscan_string(fp, color)      == EOF) return EOF;
    if (fscan_string(fp, name)       == EOF) return EOF;
    return 5;		/* # items read */
}


/****************************************************************************/
/* READ STATE FILE: obtains definition of the different states from the user*/
/*                  and places it in the state_ref array and the states     */
/*                  structure                                               */
/****************************************************************************/
boolean
ReadStatefile(trc_event_lst_p event_lst, char fname[])
{
    char        name[50], color[50];
    int         id;
    trc_event_t start;
    trc_event_t end;
    char        start_name[50], end_name[50];
    FILE       *fp;
    state_p     nextNode;
    state_p     last;


    if ((fp = fopen(fname, "r")) == NULL) {
	return FALSE;
    }

    last = NULL;
    while (read_state(fp, &id, start_name, end_name, color, name) != EOF) {

	start = trc_event_lst_find(event_lst, start_name);
	if (start == -1) {
	    fprintf(stderr, "ReadStatefile: state %d: no event \"%s\"\n",
		    id, start_name);
	    continue;
	}
	end = trc_event_lst_find(event_lst, end_name);
	if (end == -1) {
	    fprintf(stderr, "ReadStatefile: state %d: no event \"%s\"\n",
		    id, end_name);
	    continue;
	}

					/* returns the next node available or
					 * creates a new one if necessary */
	nextNode = GetStateNode(start, end, color, name, id);

	if (last == NULL) {		/* hook it up to the states queue */
	    states = nextNode;
	} else {
	    last->next = nextNode;
	}
	last = nextNode;

					/* place the corresponding state events
					 * in state_ref array for use in
					 * accessing state info later on */
	resize_array(end, &n_state_ref, (void**)&state_ref, sizeof(state_p));
	resize_array(start, &n_state_ref, (void**)&state_ref, sizeof(state_p));
	state_ref[start] = nextNode;
	state_ref[end] = nextNode;
					/* note that the relationship is made by
					 * associating the vector's index to the
					 * event# and the entry to the corresp.
					 * state id */

    }

    fclose(fp);

    return TRUE;		/* indicates file has been read in successfully
				 * and needs not be read in again */
}
