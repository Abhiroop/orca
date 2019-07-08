#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rts_init.h"
#include "precondition.h"
#include "po_rts_object.h"
#include "forest.h"
#include "misc.h"
#include "assert.h"

static int me;
static int group_size;
static int proc_debug;
static int initialized=0;

#define MODULE_NAME "RTS_INIT"

/*=======================================================================*/
/* System dependent initialization of the runtime system. */
/*=======================================================================*/
int init_rts(int *argc, char *argv[]) {
  int i;
  precondition((*argc)>0);
  precondition(argv!=NULL);
  
  if (initialized++) return 0;
  (group_size)=atoi(argv[2]); 
  assert((group_size)>0);
  (me)=atoi(argv[1]); 
  assert(((me)>=0)&&((me)<(group_size)));
  (proc_debug)=atoi(argv[3]);
  init_rts_object(me,group_size,proc_debug,sizeof(int),argc, argv);
  if (me==proc_debug) fprintf(stderr,"PO Runtime System, $Revision: 1.14 $\n");
  return 0;
}

/*=======================================================================*/
/* Clean up procedure... */
/*=======================================================================*/
int finish_rts(void) {
  if (--initialized) return 0;
  finish_rts_object();
  return 0;
}

/*=======================================================================*/
/* Unmarshalls processor number, number of processor, and processor
   debugging information from given argument structure. */
/*=======================================================================*/
int get_group_configuration(int *moi, int *gsize, int *pdebug)
{
  precondition(initialized);

  (*moi)=me;
  (*gsize)=group_size;
  (*pdebug)=proc_debug;
  return 0;
}

