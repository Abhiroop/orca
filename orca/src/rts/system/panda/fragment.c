#include <assert.h>
#include <string.h>
#include "interface.h"    /* ??? */
#include "fragment.h"
#include "continuation.h"
#include "process.h"
#include "rts_object.h"
#include "rts_trace.h"
#include "manager.h"
#include "account.h"

/* Implements object fragment interface */

/***************************/
/* Initialisation routines */
/***************************/

void
f_init(fragment_p f, tp_dscr *obj_type, char *name)
{
    f->fr_rts  = sys_malloc(sizeof(struct t_objrts));
    f->fr_type = obj_type;
    f->fr_frag = f;                   /* RAOUL: NO LONGER USED ?! */
    ro_init(&f->fr_rtsobj, name);
    ac_init(f);                       /* init accounting statistics */
}

void
f_trc_create(fragment_p f, char *trc_name)
{
    struct { short type; short cpu; rts_object_p obj; char name[80];} info;

    info.type = td_registration(f->fr_type);
    info.cpu = f->fr_oid.cpu;
    info.obj = f->fr_oid.rts;
    strncpy(info.name, trc_name, 80);

    trc_event( obj_create, (void *)&info);
}


void
f_clear(fragment_p f)
{
    ac_clear(f);                 /* end of accounting */
    ro_clear(&f->fr_rtsobj);	 /* destroy RTS-dependent part */
    f->fr_type = (tp_dscr *)0;   /* clear type */

    sys_free(f->fr_rts); 
    f->fr_rts = 0; 
}


/*********************/
/* Extracting fields */
/*********************/

#ifdef NOMACROS

f_status_t
f_get_status(fragment_p f)
{
    return (f_status_t)(f->fr_flags & RO_MASK);
}


void
f_get_oid(fragment_p f, oid_p oid)
{
    oid_copy(&(f->fr_oid), oid);
}

#endif

/***************************/
/* Read and write routines */
/***************************/

/* A return value of 1 means that the f_read completed; a return value of
 * 0 means that it did not complete.
 */
int
f_read(fragment_p f, tp_dscr *obj_type, int opindex, void **argv, source_t src)
{
    switch(opindex) {
      case READOBJ: {
	  fragment_p tmp = argv[0];

	  r_copy(tmp->fr_fields, f->fr_fields, td_objrec(obj_type));
	  ac_tick( f, AC_READ, src);
	  return 1;
      }

      case WRITEOBJ: {
	  return 0;
      }

      default: {
	  process_p self = p_self();
	  op_dscr *op    = &(td_operations(obj_type)[opindex-2]);

	  assert(op->op_read_alts || op->op_write_alts);
	  if (op->op_read_alts) {
	      p_push_object(self);
	      (*op->op_read_alts)((t_object *)f, argv);
	      if (p_pop_object(self)) {
	  	  ac_tick( f, AC_READ, src);
		  return 1;
	      }
	      return 0;
	  }
	  return 0;
      }
    }
    return 0;
}


int
f_read_write(fragment_p f, tp_dscr *obj_type, int opindex,
			 void **argv, int *modified, source_t src)
{
    switch(opindex) {
      case READOBJ: {
	  fragment_p tmp = argv[0];

	  r_copy(tmp->fr_fields, f->fr_fields, td_objrec(obj_type));
	  *modified = 0;
	  ac_tick( f, AC_READ, src);
	  return 1;
      }

      case WRITEOBJ: {
	  fragment_p tmp = argv[0];

	  r_copy(f->fr_fields, tmp->fr_fields, td_objrec(obj_type));
	  ac_tick( f, AC_WRITE, src);
	  return (*modified = 1);
      }
	
      default: {
	  process_p self = p_self();
	  op_dscr *op    = &(td_operations(obj_type)[opindex-2]);
	  
	  assert(op->op_read_alts || op->op_write_alts);
	  if (op->op_read_alts) {       /* try to read */
	      p_push_object(self);
	      (*op->op_read_alts)((t_object *)f, argv);
	      if (p_pop_object(self)) {
	          *modified = 0;
		  ac_tick( f, AC_READ, src);
		  return 1;      /* read succeeded */
	      }
	  }
	  if (op->op_write_alts) {      /* try to write */
	      p_push_object(self);
	      (*op->op_write_alts)((t_object *)f, argv);
	      if ((*modified = p_pop_object(self))) {
		  ac_tick( f, AC_WRITE, src);
		  return 1;
	      }
	      return 0;
	  }
	  return (*modified = 0);
      }
    }
}


int
f_write_read(fragment_p f, tp_dscr *obj_type, int opindex,
	     void **argv, int *modified, source_t src)
{
    switch(opindex) {
      case READOBJ: {
	  fragment_p tmp = argv[0];

	  r_copy(tmp->fr_fields, f->fr_fields, td_objrec(obj_type));
	  *modified = 0;
	  ac_tick( f, AC_READ, src);
	  return 1;
      }

      case WRITEOBJ: {
	  fragment_p tmp = argv[0];

	  r_copy(f->fr_fields, tmp->fr_fields, td_objrec(obj_type));
	  ac_tick( f, AC_WRITE, src);
	  return (*modified = 1);
      }
	
      default: {
	  process_p self = p_self();
	  op_dscr *op    = &(td_operations(obj_type)[opindex-2]);
	  
	  assert(op->op_read_alts || op->op_write_alts);
	  if (op->op_write_alts) {      /* try to write */
	      p_push_object(self);
	      (*op->op_write_alts)((t_object *)f, argv);
	      if ((*modified = p_pop_object(self))) {
		  ac_tick( f, AC_WRITE, src);
		  return 1;
	      }
	  }
	  if (op->op_read_alts) {       /* try to read */
	      p_push_object(self);
	      (*op->op_read_alts)((t_object *)f, argv);
	      if (p_pop_object(self)) {
		  ac_tick( f, AC_READ, src);
	          *modified = 0;
		  return 1;
	      }
	  }
	  return (*modified = 0);
      }
    }
}


int
f_start_read(fragment_p f)
{
    f_status_t status;
#ifdef TRACING
    return 0;                   /* Someday Ceriel will provide an "opindex" */
#endif
	
    f_lock(f);
    status = f_get_status(f);
    if (status != f_remote && status != f_in_transit) {  /* data is present */
	ac_tick( f, AC_READ, AC_LOCAL_FAST);
	f->fr_info.nr_accesses++;
	return 1;
    }
    f_unlock(f);
    return 0;
}


int
f_start_write(fragment_p f)
{
    f_status_t status;
#ifdef TRACING
    return 0;                   /* Someday Ceriel will provide an "opindex" */
#endif

    f_lock(f);
    status = f_get_status(f);
    if (status == f_unshared || status == f_owner) {
	ac_tick( f, AC_WRITE, AC_LOCAL_FAST);
        f->fr_info.nr_accesses++;
	return 1;
    }
    f_unlock(f);
    return 0;
}


void
f_end_write(fragment_p f)
{
    cont_resume(f_get_queue(f));
    f_unlock(f);
}
