#ifndef __domain__
#define __domain__

#include "po.h"
#include "instance.h"
#include "arc.h"
#include "misc.h"
#include "po_invocation.h"

/* each element from the domain contains the values for a variable,
   info if there is something to be done for that variable, and if the
   variable was modified in the current iteration
 */

typedef struct elem{
    BOOL val[MAXVAL];    /* the values of variables */
    BOOL work;           /* the variable must be checked */
    BOOL modif;          /* the variable was modified in the ct iteration*/
} ELEM, *PELEM;

typedef struct in_argum{
    int n, a;
    BOOL verbose;
} IN_ARGUM, *PIN_ARGUM;

typedef struct out_argum{
    BOOL no_solution;
    int nr_modif, con_checks;
    BOOL modif;
} OUT_ARGUM, *POUT_ARGUM;

void MaxOutArgum(POUT_ARGUM v1, POUT_ARGUM v2);


void Init_D(int sender,instance_p instance, void **args);
void Spac_Loop(int sender,instance_p instance, void **args);
void Actual_D(int sender,instance_p instance, void **args);
void Get_Print_D(int sender, instance_p instance, void **args);


int new_domain_class(int me, int gsize, int pdebug);
instance_p domain_instance(int number, handler_p handler);

#endif


