/**********************************************************/
/* Original Author: Raoul Bhoedjang. */
/* Last update by Saniya Ben Hassen: November 1995. */
/**********************************************************/

#include "po_timer.h"
#include "lock.h"
#include "misc.h"
#include "precondition.h"
#include "amoeba.h"
#include "stderr.h"
#include "sun4m_timer.h"
#include <stdio.h>
#include <stdlib.h>

#define MODULE_NAME "PO_TIMER"
#define MAXSECTIONS 100

/* This array contains the weight by which time must be multiplied to
   obtain time in a specific unit. This is system dependent. On sun4m,
   time is obtain from the system in microseconds. */
double unit_weight[] = { 1.0, 1.0/1000, 1.0/1000000, -1 };

struct po_timer_s {
    char             *at_name;   /* name of po_timer        */
    union sun4m_timer at_t0;     /* start time              */
    union sun4m_timer at_t1;     /* stop time               */
    unsigned long     at_total;  /* accumulated usecs       */
    unsigned long     at_clicks; /* call count              */
    boolean used;                /* Entry used or not used. */
};

pc_timer_type *_am_clock;

static po_timer_t po_timers[MAXSECTIONS];
static int nused;
static po_lock_p csection;
static int me, group_size, proc_debug;
static int initialized=0;

static int write_timer(FILE *fd, po_timer_t t, int label, unit_t unit);

/*==========================================================*/
/* Initializes the module. */
/*==========================================================*/
int 
init_po_timer(int moi, int gsize, int pdebug) {
  struct sun4m_timer_regs *clock_regs;
  int i;

  if ((i=init_module())<=0) return i;
  init_lock(moi,gsize,pdebug);
  csection=new_lock();
  /*
   * Initialise the clock pointer. Panda may already have
   * initialised the microsecond po_timer, so don't check result.
   */
  (void) sun4m_timer_init(NULL);
  clock_regs = sun4m_timer_getptr();
  _am_clock = &clock_regs->pc_timer;
  for (i=0; i<MAXSECTIONS; i++) po_timers[i].used=FALSE;
  return 0;
}


/*====================================================================*/
/* Finishes the module. */
/*====================================================================*/
int 
finish_po_timer() {
  if (--initialized) return 0;
  free_lock(csection);
  finish_lock();
  return 0;
}

/*====================================================================*/
/* Creates a po_timer. */
/*====================================================================*/

po_timer_p 
new_po_timer(char *name) {
  int i;
  
  precondition(initialized);
  if (nused==MAXSECTIONS) return NULL;
  lock(csection);
  for (i=0; i<MAXSECTIONS; i++) 
    if (!(po_timers[i].used)) break;
  sys_error(i>=MAXSECTIONS);
  nused++;
  po_timers[i].used=TRUE;
  unlock(csection);
  po_timers[i].at_name = name;
  return &(po_timers[i]);
}

/*====================================================================*/
/* Frees a po_timer. */
/*====================================================================*/

int 
free_po_timer(po_timer_p po_timer) {
  precondition(po_timer!=NULL);
  po_timer->used=FALSE;
  nused--;
  return 0;
}

/*====================================================================*/
/* Starts timing. */
/*====================================================================*/
int 
start_po_timer(po_timer_p sp) {
  copy_timer(_am_clock, &((sp)->at_t0.tm_val));
  return 0;
}

/*====================================================================*/
/* Ends timing. */
/*====================================================================*/
int 
end_po_timer(po_timer_p sp) {
  copy_timer(_am_clock, &((sp)->at_t1.tm_val));
  return 0;
}

/*====================================================================*/
/* Adds last timing to accumulated time. */
/*====================================================================*/
int 
add_po_timer(po_timer_p sp) {
  (sp)->at_total += sun4m_timer_diff(&((sp)->at_t0), 
				     &((sp)->at_t1));
  (sp)->at_clicks++; 
  return 0;
}

/*====================================================================*/
/* Resets accumulated time to 0. */
/*====================================================================*/
int 
reset_po_timer(po_timer_p sp) {
  precondition(initialized);
  precondition(sp!=NULL);
  (sp)->at_total=0; 
  (sp)->at_clicks=0; 
  return 0;
}

/*====================================================================*/
/* Substracts the start time of a po_timer from the start time of another
   po_timer. */
/*====================================================================*/
po_time_t 
sub_po_timer(po_timer_p sp1, po_timer_p sp2, unit_t unit) {
  return (po_time_t )(sun4m_timer_diff(&((sp1)->at_t0), &((sp2)->at_t0))
		   *unit_weight[unit]);
}

/*====================================================================*/
/* Returns the last timed section. */
/*====================================================================*/
po_time_t 
read_last(po_timer_p sp, unit_t unit) {
  return (po_time_t )(sun4m_timer_diff(&((sp)->at_t0),&((sp)->at_t1))
		   *unit_weight[unit]);
}

/*====================================================================*/
/* Returns the accumulated time. */
/*====================================================================*/
po_time_t 
read_total(po_timer_p sp, unit_t unit) {
  return (po_time_t )((sp)->at_total*unit_weight[unit]);
}

/*====================================================================*/
/* Prints a summary of the timing of one section on a file. The
   arguments to this procedure are a filename, to which 'me' is
   appended. The tag is printed as the first item on each line. After
   printing the po_timer's values, this procedure resets the po_timer. */
/*====================================================================*/
int 
log_po_timer(po_timer_p sp, char *filename, int tag, unit_t unit) {
  char string[256];
  FILE *at_dump_file;
  
  precondition(initialized);
  precondition(sp!=NULL);
  precondition(filename!=NULL);

  if (sp->at_clicks>0) {
    sprintf(string,"%s.timers%d",filename,me);
    at_dump_file=fopen(string,"a");
    if (at_dump_file==NULL) return -1;
    write_timer(at_dump_file, *sp, tag, unit);
    fclose(at_dump_file);
  }
  return 0;
}

/*====================================================================*/
/* Prints a summary of the timing of each section on a file. The
   arguments to this procedure are a filename, to which 'me' is
   appended. The tag is printed as the first item on each line, for
   each section. After printing a po_timer's values, this procedure
   resets the po_timer. */
/*====================================================================*/
int 
log_all_po_timers(char *filename, int tag, unit_t unit) {
  int i;
  char string[256];
  FILE *at_dump_file;
  
  precondition(initialized);
  precondition(filename!=NULL);
  
  sprintf(string,"%s.timers%d",filename,me);
  at_dump_file=fopen(string,"a");
  if (at_dump_file==NULL) return -1;
  
  for (i = 0; i < MAXSECTIONS; i++) 
    if ((po_timers[i].used) && (po_timers[i].at_clicks>0)) {
      write_timer(at_dump_file,po_timers[i], tag, unit);
    }
  fclose(at_dump_file);
  return 0;
}

int
write_timer(FILE *fd, po_timer_t t, int label, unit_t unit) {
  int r;

  r=fprintf(fd, "%5d \t %15.15s %15.5f \t %5lu clicks\t %15.5f per_click\n",
	    label, t.at_name, t.at_total*unit_weight[unit], 
	    t.at_clicks,
	    ((float)(t.at_total*unit_weight[unit])
	     /(float)(t.at_clicks)));
  fflush(fd);
  t.at_total=0;
  t.at_clicks=0;
  return r;
}
