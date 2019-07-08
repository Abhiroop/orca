/* $Id: args.c,v 1.7 1996/07/04 08:52:49 ceriel Exp $ */

#include <interface.h>

void **
copy_args(prc_dscr *prc, void **argtab)
{
  int	nparams = td_nparams(prc->prc_func);
  register par_dscr *params = td_params(prc->prc_func);
  void **args = 0;
  void **p;
  register int i;

  if (nparams) {
	args = p = (void **) m_malloc(nparams * sizeof(void *));
	for (i = 0; i < nparams; i++) {
		void *a = argtab[i];
		*p = m_malloc(params->par_descr->td_size);
		if (params->par_mode == SHARED) {
		    memcpy(*p, a, params->par_descr->td_size);
		    if (params->par_descr->td_type == ARRAY) {
			t_array *ap = (t_array *) *p;
			t_object *o;
			int j;

			if (ap->a_sz <= 0) {
			    continue;
			}
			o = m_malloc(ap->a_sz * sizeof(t_object));
			ap->a_data = (char *) o -
					ap->a_offset * sizeof(t_object);
			memcpy(o, 
			       (char *) ((t_array *) a)->a_data + ap->a_offset * sizeof(t_object), 
			       ap->a_sz * sizeof(t_object));
			for (j = ap->a_sz; j > 0; j--, o++) {
			    o->o_shared++;
			    o->o_refcount++;
#ifdef OPTIMIZED
			    o->o_state[this_cpu].o_refcnt++;
			    o->o_state[this_cpu].o_score +=
				prc->prc_shargs[i].arg_score;
			    o->o_state[this_cpu].o_naccess +=
				prc->prc_shargs[i].arg_naccess;
#endif
			}
		    }
		    else {
			((t_object *) a)->o_shared++;
			((t_object *) a)->o_refcount++;
#ifdef OPTIMIZED
			((t_object *) a)->o_state[this_cpu].o_refcnt++;
			((t_object *) a)->o_state[this_cpu].o_score +=
				prc->prc_shargs[i].arg_score;
			((t_object *) a)->o_state[this_cpu].o_naccess +=
				prc->prc_shargs[i].arg_naccess;
#endif
		    }
		}
		else {
			memset(*p, '\0', params->par_descr->td_size);
			(*(params->par_copy))(*p, a);
		}
		p++;
		params++;
	}
  }
  return  args;
}

void
free_args(prc_dscr *prc, void **args)
{
  int	nparams = td_nparams(prc->prc_func);
  register par_dscr *params = td_params(prc->prc_func);
  void **p;
  register int i;

  if (nparams) {
	p = args;
	while (nparams--) {
		/* Don't clean up objects that have been shared. Gives too
		   much trouble when Orca processes exit and grp_listener
		   has a reference to the object ...
		   See also the test on o_shared in o_free.
		   If we put this in again, also update the code for arrays
		   of objects ...

		if (params->par_mode == SHARED) {
			((t_object *) *p)->o_shared--;
		}
		*/
		if (params->par_mode == SHARED) {
			if (params->par_free) {
				(*(params->par_free))(*p);
			}
		}
		m_free(*p);
		params++;
		p++;
	}
	m_free(args);
  }
}
