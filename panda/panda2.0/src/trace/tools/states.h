#ifndef __UPSHOT_STATES_H__
#define __UPSHOT_STATES_H__


#include "pan_sys.h"
#include "pan_util.h"

#include "pan_trace.h"
#include "trc_event_tp.h"



/*-- Types -------------------------------------------------------------------*/


typedef struct STATE		state_t, *state_p;
			/* Opaque type */

struct STATE {			/* state info */
    trc_event_t  start;		/* event type that starts this state */
    trc_event_t  end;		/* event type that ends this state */
    int          color;		/* color index */
    char        *color_name;	/* color name */
    char        *name;		/* state name */
    state_p      next;		/* link in state list */
    int          pattern;	/* black & white pattern iso color */
};



/*--  Public arrays and pointers ------------------------------------------- **/



/*-- GENERAL GLOBALS ---------------------------------------------------------*/


/*-- EXPORTED FUNCTIONS ------------------------------------------------------*/


state_p     first_state(void);

state_p     next_state(state_p prev_state);

int         num_states(void);

state_p     state_locate(trc_event_t event);

void        start_state_module(void);

void        FreeStateQueue(void);

boolean     ReadStatefile(trc_event_lst_p event_lst, char fname[]);


#endif
