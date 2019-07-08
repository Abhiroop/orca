#include <stdio.h>
#include <stdlib.h>
#include "rts_init.h"
#include "precondition.h"
#include "po_rts_object.h"
#include "misc.h"

static int me;
static int group_size;
static int proc_debug;
static boolean initialized=FALSE;

#define MODULE_NAME "RTS_INIT"

/*=======================================================================*/
/* System dependent initialization of the runtime system. */
/*=======================================================================*/
int init_rts(int *argc, char *argv[]) {
  int r;
  int moi, gsize, pdebug;

  precondition((*argc)>0);
  precondition(argv!=NULL);
  
  (gsize)=atoi(argv[2]);
  assert((gsize)>0);
  (moi)=atoi(argv[1]);
  assert(((moi)>=0)&&((moi)<(gsize)));
  (pdebug)=atoi(argv[3]);
  if ((r=init_module())<0) return -1;
  if (initialized) return 0;
  if (me==proc_debug) fprintf(stderr,"PO Runtime System, $Revision: 1.2 $\n");
  initialized=TRUE;
  pan_init(argc, argv);
  pan_mp_init();
  init_mp_channel(moi, gsize, pdebug);
  return 0;
}

/*=======================================================================*/
/* Clean up procedure... */
/*=======================================================================*/
int finish_rts(void) {
  if (!initialized) return 0;
  initialized=FALSE;
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

