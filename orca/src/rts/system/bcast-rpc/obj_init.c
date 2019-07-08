/* $Id: obj_init.c,v 1.13 1996/07/04 08:52:57 ceriel Exp $ */

#include <interface.h>
#include "module/mutex.h"

extern int ncpus;
#ifdef CHK_FORK_MSGS
extern int chksums;
#endif

void
o_init_rtsdep(t_object *o, tp_dscr *d, char *n)
{
	o->o_rtsdep = m_malloc(sizeof (struct t_objrts));
	memset((void *) (o->o_rtsdep), 0, sizeof(struct t_objrts));
	o->o_refcount = 1;
	InstallObject(o);
	mu_init(&o->o_mutex);
	o->o_replicated = 1;
	o->o_owner = -1;
	o->o_dscr = d;
#ifdef OPTIMIZED
	o->o_state = (t_obj_state *) m_malloc(sizeof(t_obj_state) * ncpus);
	memset((void *) (o->o_state), 0, sizeof(t_obj_state) * ncpus);
	o->o_state[this_cpu].o_replica = 1;
	o->o_state[this_cpu].o_refcnt = 1;
	sema_init(&o->o_semdata, 0);
#ifdef CHK_FORK_MSGS
	if (chksums) {
		register int i;
		for (i = 0; i < ncpus; i++) {
			o->o_state[i].o_uncertainty = i;
		}
	}
#endif
#endif
}

void
o_kill_rtsdep(t_object *o)
{
	DeleteObject(o);
#ifdef OPTIMIZED
	if (o->o_shared) {
		stat_keep_descr(o);
	}
	else {
		m_free((void *) o->o_state);
		m_free(o->o_rtsdep);
	}
#else
	m_free(o->o_rtsdep);
#endif
}

int o_free(t_object *o)
{
  if (! o->o_rtsdep) return 0;
  mu_lock(&o->o_mutex);
  o->o_refcount--;
  if (o->o_refcount == 0 && ! o->o_shared) {
  	mu_unlock(&o->o_mutex);
  	o_kill_rtsdep(o);
	return 1;
  }
  else {
	mu_unlock(&o->o_mutex);
  }
  o->o_fields = 0;
  return 0;
}
