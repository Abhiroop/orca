/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

#ifndef FRAGMENT_INLINE
#define FRAGMENT_SRC
#endif
#include <assert.h>
#include <string.h>		/* need prototype for strncpy RFHH */
#include "interface.h"		/* For BLOCKING and NESTED */
#include "continuation.h"
#include "fragment.h"
#include "process.h"
#include "rts_globals.h"
#include "rts_internals.h"
#include "rts_measure.h"
#include "rts_object.h"
#include "rts_trace.h"
#include "manager.h"
#include "account.h"
#ifdef PURE_WRITE
#include "proxy.h"
#endif

#ifdef FRAGMENT_SRC
#undef INLINE_FUNCTIONS
#endif
#include "inline.h"

/* Implements object fragment interface */

#ifdef OUTLINE
/***************************/
/* Initialisation routines */
/***************************/

void
f_init(fragment_p f, tp_dscr *obj_type, char *name)
{
    f->fr_rts  = (struct t_objrts *)m_malloc(sizeof(struct t_objrts));
    f->fr_type = obj_type;
    f->fr_frag = f;
    ro_init(f, name);
    ac_init(f);                       /* init accounting statistics */
}

void
f_trc_create(fragment_p f, char *trc_name)
{
#ifdef TRACING
    struct { short type; short cpu; fragment_p obj; char name[80];} info;

    info.type = td_registration(f->fr_type);
    info.cpu = f->fr_home;
    info.obj = f->fr_home_obj;

    strncpy(info.name, trc_name, 80);

    trc_event( obj_create, (void *)&info);
#endif
}

void
f_clear(fragment_p f)
{
    ac_clear(f);         /* destroy and print accounting info */
    ro_clear(f);	 /* destroy RTS-dependent part */
    f->fr_type = 0;      /* clear type */

    m_free(f->fr_rts);
    f->fr_rts = 0; 
}


/******************************************************/
/* Routines that support inline execution by compiler */
/******************************************************/

int
f_start_read(fragment_p f)
{
    f_status_t status;

#ifdef TRACING
    /* We should not grant permission here (see f_start_write), but some
     * applications (tsp, awari) drown in local reads so we support an option
     * that neglects *all* local read operations.
     */
    if (trc_local_reads)
	return 0;
#endif

    rts_lock();
    status = f_get_status(f);
    if (status != f_remote && status != f_in_transit) {  /* data is present */
	ac_tick( f, AC_READ, AC_LOCAL_FAST);
	f->fr_info.nr_accesses++;
	return 1;
    }

    rts_unlock();
    return 0;
}

int
f_start_write(fragment_p f)
{
    f_status_t status;


#ifdef TRACING
    /*
     * If we let f_start_write succeed here, then we cannot
     * tell the tracing package what operation is taking place.
     * Therefore, we return 0, which will cause DoOperation
     * to be called. DoOperation gets all the information
     * that is needed by the tracing package.
     *
     * Someday Ceriel will provide an "opindex."
     */
    return 0;
#endif

    rts_lock();

    status = f_get_status(f);
    if (status == f_unshared || status == f_owner) {
	ac_tick( f, AC_WRITE, AC_LOCAL_FAST);
	f->fr_info.nr_accesses++;
	return 1;
    }

    rts_unlock();
    return 0;
}

void
f_end_write(fragment_p f)
{
    cont_resume(f_get_queue(f));
    rts_unlock();
}

#endif

/***************************/
/* Read and write routines */
/***************************/


/* A return value of 1 means that the f_read completed; a return value of
 * 0 means that it did not complete.
 */
INLINE int
f_read(fragment_p f, int *op_flags, op_dscr *op, void **argv, source_t src)
{
    assert(op->op_read_alts || op->op_write_alts);
    if (op->op_read_alts) {
#ifdef PURE_WRITE
	rts_flush_pure_writes(p_self(), f, 1);
#endif
	*op_flags &= ~BLOCKING;
	if (! (*op->op_read_alts)((t_object *)f, argv)) {
		  ac_tick( f, AC_READ, src);
		  return 1;
	}
	if (*op_flags & NESTED) *op_flags |= BLOCKING;
    }
    return 0;
}

INLINE int
f_read_write(fragment_p f, int *op_flags, op_dscr *op,
	     void **argv, int *modified, source_t src)
{
  assert(op->op_read_alts || op->op_write_alts);
  if (op->op_read_alts) {       /* try to read */
      *op_flags &= ~BLOCKING;
      if (! (*op->op_read_alts)((t_object *)f, argv)) {
	  *modified = 0;
	  ac_tick( f, AC_READ, src);
	  return 1;      /* read succeeded */
      }
  }
  if (op->op_write_alts) {      /* try to write */
/* extern rts_timer_p compiled_timer; */
/* 	      rts_measure_enter(compiled_timer); */
      *op_flags &= ~BLOCKING;
      if (! (*op->op_write_alts)((t_object *)f, argv)) {
/* 	      rts_measure_leave(compiled_timer); */
	  *modified = 1;
	  ac_tick( f, AC_WRITE, src);
	  return 1;
      }
/* 	      rts_measure_leave(compiled_timer); */
      return 0;
  }
  *modified = 0;
  if (*op_flags & NESTED) *op_flags |= BLOCKING;
  return 0;
}

INLINE int
f_write_read(fragment_p f, int *op_flags, op_dscr *op,
	     void **argv, int *modified, source_t src)
{
  assert(op->op_read_alts || op->op_write_alts);
  if (op->op_write_alts) {      /* try to write */
      *op_flags &= ~BLOCKING;
      if (! (*op->op_write_alts)((t_object *)f, argv)) {
	  *modified = 1;
	  ac_tick( f, AC_WRITE, src);
	  return 1;
      }
  }
  if (op->op_read_alts) {       /* try to read */
      *op_flags &= ~BLOCKING;
      if (! (*op->op_read_alts)((t_object *)f, argv)) {
	  ac_tick( f, AC_READ, src);
	  *modified = 0;
	  return 1;
      }
  }
  if (*op_flags & NESTED) *op_flags |= BLOCKING;
  return (*modified = 0);
}

#undef FRAGMENT_SRC
