/*
 * Author:         Tim Ruhl
 *
 * Date:           Nov 30, 1995
 *
 * Partitioned object timer module.
 */

#include "synchronization.h"    /* Part of the synchronization module */
#include "panda_po_timer.h"
#include "pan_sys.h"
#include "util.h"

#include <string.h>
#include <assert.h>

static po_timer_p active;

/* Global time variable */
static int initialized;
static pan_time_p  time;
static pan_mutex_p time_lock;

int 
init_po_timer(int me, int group_size, int proc_debug)
{
    if (initialized++) return 0;

    time = pan_time_create();
    time_lock = pan_mutex_create();

    return 0;
}

int 
finish_po_timer(void)
{
    if (--initialized) return 0;

    pan_time_clear(time);
    pan_mutex_clear(time_lock);
    
    return 0;
}


po_timer_p 
new_po_timer(char *name)
{
    po_timer_p t;

    t = pan_malloc(sizeof(po_timer_t));

    t->name = pan_malloc(strlen(name) + 1);
    strcpy(t->name, name);

    t->time = pan_time_create();
    t->total = pan_time_create();
    pan_time_set(t->total, 0L, 0UL);
    t->clicks = 0;

    t->next = active;
    active = t;

    return t;
}

int 
free_po_timer(po_timer_p t)
{
    po_timer_p *p;

    /* Unlink timer from active list */
    for(p = &active; *p != t; p = &((*p)->next))
	;
    assert(*p == t);

    *p = t->next;

    pan_free(t->name);
    pan_time_clear(t->time);
    pan_time_clear(t->total);

    pan_free(t);

    return 0;
}

int 
start_po_timer(po_timer_p t)
{
    pan_time_get(t->time);

    return 0;
}

int 
end_po_timer(po_timer_p t)
{
    pan_mutex_lock(time_lock);
    pan_time_get(time);
    pan_time_sub(time, t->time);
    pan_time_copy(t->time, time);
    pan_mutex_unlock(time_lock);

    return 0;
}

int 
add_po_timer(po_timer_p t)
{
    pan_time_add(t->total, t->time);
    t->clicks++;

    return 0;
}

int 
reset_po_timer(po_timer_p t)
{
    pan_time_set(t->total, 0L, 0UL);
    t->clicks = 0;

    return 0;
}

po_time_t 
sub_po_timer(po_timer_p base, po_timer_p sub, unit_t unit)
{
    po_time_t t;

    pan_mutex_lock(time_lock);

    pan_time_copy(time, base->time);
    pan_time_sub(time, sub->time);

    t = rts_po_timer_panda2time(time, unit);
    
    pan_mutex_unlock(time_lock);

    return t;
}


po_time_t 
read_last(po_timer_p t, unit_t unit)
{
    po_time_t res;

    res = rts_po_timer_panda2time(t->time, unit);

    return res;
}

po_time_t 
read_total(po_timer_p t, unit_t unit)
{
    po_time_t res;

    res = rts_po_timer_panda2time(t->total, unit);

    return res;
}

/*
 * write_timer:
 *                 Write timer t to file fp and reset the timer.
 */
static void
write_timer(FILE *fp, po_timer_p t, int label, unit_t unit)
{
    double sec;

    sec = pan_time_t2d(t->total);

    switch(unit) {
    case PO_MICROSEC:
	sec = sec * 1000000.0;
	break;
    case PO_MILLISEC:
	sec = sec * 1000.0;
	break;
    case PO_SEC:
	break;
    default:
	rts_error("Unknown unit type: %d\n", unit);
	break;
    }

    if (t->clicks) {
	fprintf(fp,
		"%5d \t %20.20s: %10.2f \t %10lu clicks\t %10.2f per_click\n",
            	label, t->name, sec, t->clicks, sec / t->clicks);
    }

    reset_po_timer(t);
}

int 
log_po_timer(po_timer_p t, char *filename, int label, unit_t unit)
{
    char buf[256];
    FILE *fp;

    if (t->clicks == 0) return 0;

    sprintf(buf, "%s%d", filename, pan_my_pid());
    fp = fopen(buf, "a");
    if (fp == NULL) rts_error("Cannot open timer log file %s\n", buf);

    write_timer(fp, t, label, unit);
    
    fclose(fp);

    return 0;
}

int 
log_all_po_timers(char *filename, int label, unit_t unit)
{
    char buf[256];
    po_timer_p p;
    FILE *fp;

    sprintf(buf, "%s%d", filename, pan_my_pid());
    fp = fopen(buf, "a");
    if (fp == NULL) rts_error("Cannot open timer log file %s\n", buf);
    
    for(p = active; p; p = p->next) {
	write_timer(fp, p, label, unit);
    }

    fclose(fp);
    
    return 0;
}

void 
rts_po_timer_time2panda(po_time_t time, unit_t unit, pan_time_p p)
{
    unsigned long nsec;
    long sec;

    switch(unit) {
    case PO_MICROSEC:
	sec = time / 1000000;
	nsec = (time - sec * 1000000) * 1000;
	break;
    case PO_MILLISEC:
	sec = time / 1000;
	nsec = (time - sec * 1000) * 1000000;
	break;
    case PO_SEC:
	sec = time;
	nsec = 0;
	break;
    default:
	rts_error("Unknown unit type: %d\n", unit);
	break;
    }

    pan_time_set(p, sec, nsec);
}

po_time_t
rts_po_timer_panda2time(pan_time_p p, unit_t unit)
{
    po_time_t t;
    double sec;

    sec = pan_time_t2d(p);

    switch(unit) {
    case PO_MICROSEC:
	t = (po_time_t)(sec * 1000000.0);
	break;
    case PO_MILLISEC:
	t = (po_time_t)(sec * 1000.0);
	break;
    case PO_SEC:
	t = (po_time_t)sec;
	break;
    default:
	rts_error("Unknown unit type: %d\n", unit);
	break;
    }

    return t;
}
