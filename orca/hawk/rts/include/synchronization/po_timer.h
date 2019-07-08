/*********************************************************************/
/*********************************************************************/
/* Time abstract data type. */
/*********************************************************************/
/*********************************************************************/

#ifndef __po_timer__
#define __po_timer__

typedef enum {
  PO_MICROSEC,
  PO_MILLISEC,
  PO_SEC,
  num_unit_t
} unit_t;

typedef struct po_timer_s po_timer_t, *po_timer_p;
typedef double po_time_t;

int init_po_timer(int me, int group_size, int proc_debug);
int finish_po_timer(void);

po_timer_p new_po_timer(char *name);
int free_po_timer(po_timer_p);
int start_po_timer(po_timer_p);
int end_po_timer(po_timer_p);
int add_po_timer(po_timer_p);
int reset_po_timer(po_timer_p);
po_time_t sub_po_timer(po_timer_p, po_timer_p, unit_t unit);

po_time_t read_last(po_timer_p, unit_t unit);
po_time_t read_total(po_timer_p, unit_t unit);
int log_po_timer(po_timer_p sp, char *filename, int label, unit_t unit);
int log_all_po_timers(char *filename, int label, unit_t unit);

#endif
