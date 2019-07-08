/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* L I V E   V A R I A B L E S	 A N D	 C O M B I N I N G */

/* This optimizer pass is used to combine temporaries. The compiler produces
   many temporaries, and there are many opportunities to combine them.
   The C compiler could combine most of those as well, but temporaries that
   contain more complex values are only freed at the end of a procedure,
   so in that case the C compiler won't notice that it can combine those.

   The algorithm of this optimizer phase is as follows:
   - first compute for all variables X the program points P where they are,
     "live", t.i. wether the value of X at P could be used along some
     path in the flow-graph starting at P.
   - then, we can combine two variables when they have the same type and
     their "live" ranges are disjunct.

   For now, we only do this for temporaries.
   Future possible extension: also do this for Orca program variables.
*/

/* $Id: opt_LV.c,v 1.11 1998/03/06 15:39:59 ceriel Exp $ */

#include "ansi.h"
#include "debug.h"
#include <assert.h>

#ifdef DEBUG
#include <stdio.h>
#include "gen_expr.h"
#endif

#include "opt_LV.h"
#include "sets.h"
#include "def.h"
#include "node.h"
#include "node_num.h"
#include "bld_graph.h"
#include "temps.h"
#include "oc_stds.h"
#include "visit.h"
#include "options.h"
#include "generate.h"
#include "scope.h"

_PROTOTYPE(static int live_stat, (int, int));
_PROTOTYPE(static int live_meet, (int, int *));
_PROTOTYPE(static int mark_used, (p_node));
_PROTOTYPE(static int mark_def, (p_node));
_PROTOTYPE(static int compute_defused, (p_node));
_PROTOTYPE(static int replace_var, (p_node));
_PROTOTYPE(static void LV_compute, (p_node));
_PROTOTYPE(static void change_bats, (t_def *));

static struct LV_sets {
	p_set	def;		/* DEF: definitely defined before used. */
	p_set	used;		/* USED: may be defined before used. */
	p_set	live;		/* LIVE: see comment above. */
} *LV_sets;			/* One for each statement. */

static int
	tset_sz;		/* Size of sets: depends on # of variables. */
static p_set
	temp_set;		/* Just a temporary. */
static int
	endstatno;
#define beginstatno	0
				/* Statement numbering. */
static int
	*var_map;		/* Will map variables onto other variables:
				   var_map[i] == j means that variable i is
				   replaced by variable j.
				*/
static void
	**var_tab;		/* Will map variable-numbering onto variables.
				   May be used for temporaries as well as orca
				   variables.
				*/

void do_LV(fdf)
	t_def	*fdf;
{
	/* Perform the optimization on function/operation/process fdf.
	*/
	int	n;
	t_tmp	*tmp, *prev;
	p_node	nd;
	p_set	*tmpalive_sets;
	struct LV_sets
		*p;
	int	ssiz;
	int	i, j;

	/* Number temporaries. */
	n = 1;
	for (tmp = fdf->bod_temps; tmp; tmp = tmp->tmp_scopelink) {
		tmp->tmp_num = n++;
	}

	tset_sz = set_size(n);

	/* If there are none or only one, return. */
	if (n < 3) return;

	/* Allocate and initialize var_* tables. */
	var_map = (int *) Malloc(n * sizeof(int));
	var_tab = (void **) Malloc(n * sizeof(void *));
	for (i = 1, tmp = fdf->bod_temps;
	     tmp;
	     i++, tmp = tmp->tmp_scopelink) {
		var_map[i] = 0;
		var_tab[i] = tmp;
	}

	/* We want LV_compute to see the complete body of the operation,
	   se we create an extra node that links the read and write
	   alternatives. If either is empty, this is no problem.
	   Also link in the initializers.
	*/
	nd = mk_leaf(Stat, ',');
	nd->nd_list1 = fdf->bod_statlist1;
	nd->nd_list2 = fdf->bod_statlist2;
	if (fdf->bod_init) {
		p_node	nd1 = mk_leaf(Stat, ',');
		nd1->nd_list1 = nd;
		nd1->nd_list2 = fdf->bod_init;
		nd = nd1;
	}
	if (fdf->opr_dependencies) {
		p_node	nd1 = mk_leaf(Stat, ',');
		nd1->nd_list1 = nd;
		nd1->nd_list2 = fdf->opr_dependencies;
		nd = nd1;
	}

	/* Now, compute LIVE information. */
	LV_compute(nd);

#ifdef DEBUG
	/* Debugging output. */
	if (options['o']) {
		printf("LV, body of %s:\n", fdf->df_idf->id_text);
		dump_node(nd, 0);
		for (tmp = fdf->bod_temps; tmp; tmp = tmp->tmp_scopelink) {
			printf("%s, ", fdf->df_idf->id_text);
			printf(TMP_FMT, tmp->tmp_id);
			printf("\n");
			for (j = beginstatno; j <= endstatno; j++) {
				printf("    statement %d", j);
				p = &(LV_sets[j]);
				if (set_ismember(p->used, tmp->tmp_num)) {
					printf(", USED");
				}
				if (set_ismember(p->def, tmp->tmp_num)) {
					printf(", DEF");
				}
				if (set_ismember(p->live, tmp->tmp_num)) {
					printf(", LIVE");
				}
				printf("\n");
			}
		}
	}
#endif

	/* Now collect LIVE information per temporary (we have it per statement,
	   but want it the other way around).
	*/
	tmpalive_sets = (p_set *) Malloc(n * sizeof(p_set));
	ssiz = set_size(endstatno+1);
	for (i = 1; i < n; i++) {
		tmpalive_sets[i] = set_create(ssiz);
		set_init(tmpalive_sets[i], 0, ssiz);
		for (p = &(LV_sets[endstatno]), j = endstatno;
		     p >= &(LV_sets[beginstatno]);
		     p--, j--) {
			if (set_ismember(p->live, i)) {
				set_putmember(tmpalive_sets[i], j);
			}
		}
	}

	/* Create temporary set. */
	temp_set = set_create(ssiz);

	/* Now check for each temporary which other temporaries have the same
	   type and a disjunct live-range. These can be combined.
	*/
	tmp = fdf->bod_temps;
	for (i = 1, tmp = fdf->bod_temps;
	     tmp;
	     i++, tmp = tmp->tmp_scopelink) {
		t_tmp	*tmp2;

		if (var_map[i]) continue;
		for (j = i+1, tmp2 = tmp->tmp_scopelink;
		     j < n;
		     j++, tmp2 = tmp2->tmp_scopelink) {
			if (var_map[j] ||
			    tmp2->tmp_type != tmp->tmp_type ||
			    tmp2->tmp_ispointer != tmp->tmp_ispointer) continue;
			(void) set_assign(temp_set, tmpalive_sets[i], ssiz);
			(void) set_intersect(temp_set,
					     temp_set,
					     tmpalive_sets[j],
					     ssiz);
			if (set_isempty(temp_set, ssiz)) {
				(void) set_union(tmpalive_sets[i],
						 tmpalive_sets[i],
						 tmpalive_sets[j],
						 ssiz);
				var_map[j] = i;
			}
		}
	}

	/* Now actually replace, in code as well as in bounds-and-tag
	   expressions.
	*/
	visit_node(nd, Tmp|Def|Call, replace_var, 0);
	walkdefs(fdf->bod_scope->sc_def, D_VARIABLE|D_TYPE, change_bats);

	/* Cleanup temporaries that are no longer used. */
	tmp = fdf->bod_temps;
	prev = 0;
	while (tmp) {
		t_tmp	*tmp1 = tmp->tmp_scopelink;
		if (var_map[tmp->tmp_num]) {
			if (prev) {
				prev->tmp_scopelink = tmp1;
			}
			else {
				fdf->bod_temps = tmp1;
			}
			free_tmp(tmp);
		}
		else prev = tmp;
		tmp = tmp1;
	}

	/* Cleanup. */
	free_node(nd);
	set_free(temp_set, ssiz);
	for (i = 1; i < n; i++) {
		set_free(tmpalive_sets[i], ssiz);
	}
	free(tmpalive_sets);
	for (p = &(LV_sets[endstatno]); p >= &(LV_sets[beginstatno]); p--) {
		set_free(p->used, tset_sz);
		set_free(p->def, tset_sz);
		set_free(p->live, tset_sz);
	}
	free(LV_sets);
	free(var_map);
	free(var_tab);
}

static void
change_bats(df)
	t_def	*df;
{
	/* Implement the mapping of temporaries in the bounds-and-tags
	   expressions associated with the declaration of df.
	*/
	t_type	*tp;
	int	i;

	/* Variables, fields, et cetera. */
	switch(df->df_kind) {
	case D_VARIABLE:
		if (! (df->df_flags & D_PART_INDEX) && df->var_bat) {
			visit_ndlist(df->var_bat, Tmp|Def|Call, replace_var, 0);
		}
		break;
	case D_FIELD:
	case D_OFIELD:
	case D_UFIELD:
		if (df->fld_bat) {
			visit_ndlist(df->fld_bat, Tmp|Def|Call, replace_var, 0);
		}
		break;
	}

	/* Types and anonymous types used in variable or field declarations. */
	tp = df->df_type;
	if (df->df_kind == D_TYPE || tp->tp_anon) {
		switch(tp->tp_fund) {
		case T_RECORD:
			walkdefs(tp->rec_scope->sc_def,
				 D_FIELD,
				 change_bats);
			break;
		case T_GRAPH:
			walkdefs(tp->gra_root->rec_scope->sc_def,
				 D_FIELD,
				 change_bats);
			walkdefs(tp->gra_node->rec_scope->sc_def,
				 D_FIELD,
				 change_bats);
			break;
		case T_UNION:
			walkdefs(tp->rec_scope->sc_def,
				 D_UFIELD,
				 change_bats);
			if (tp->rec_init) {
				visit_node(tp->rec_init,
					   Tmp|Def|Call,
					   replace_var,
					   0);
			}
			break;
		case T_OBJECT:
			walkdefs(record_type_of(tp)->rec_scope->sc_def,
				 D_OFIELD,
				 change_bats);
			break;
		case T_ARRAY:
			for (i = 0; i < tp->arr_ndim; i++) {
				if (tp->arr_bounds(i)) {
					visit_node(tp->arr_bounds(i),
						   Tmp|Def|Call,
						   replace_var,
						   0);
				}
			}
			break;
		}
	}
}

static void LV_compute(statlist)
	p_node	statlist;
{
	/* Perform the computations associated with this optimization.
	*/
	int	i;
	struct LV_sets
		*p;
	dfl_graph
		grph;
	p_node	*slist;

	/* First, number statement nodes, so that we know how many sets
	   of what size to allocate.  Also, an extra position is added
	   at begin and end (0 and endstatno).
	*/
	endstatno = number_statements(statlist, &slist) + 1;

	/* Allocate sets. */
	LV_sets = (struct LV_sets *)
		Malloc((endstatno+1) * sizeof(struct LV_sets));
	for (p = &(LV_sets[endstatno]); p >= &(LV_sets[beginstatno]); p--) {
		p->used = set_create(tset_sz);
		set_init(p->used, 0, tset_sz);
		p->def = set_create(tset_sz);
		set_init(p->def, 0, tset_sz);
		p->live = set_create(tset_sz);
		set_init(p->live, 0, tset_sz);
	}
	temp_set = set_create(tset_sz);

	/* Now, compute the flow graph. */
	grph = dfl_buildgraph(endstatno+1, beginstatno, statlist);

	/* Then, compute local predicates:
	   USED: set when temporary may be used in a statement,
	   before being set. Simplified to all uses.
	   DEF: set when a temporary is definitely set in
	   a statement before being used.
	*/
	visit_ndlist(statlist, Stat|Call, compute_defused, 0);
	for (p = &(LV_sets[endstatno]); p >= &(LV_sets[beginstatno]); p--) {
		(void) set_minus(p->def, p->def, p->used, tset_sz);
	}

	/* Compute LIVE. Least solution is required, so live sets are
	   initialized empty (see above).
	*/
	dfl_backward_flow(grph, live_meet, live_stat);

	/* Cleanup, but keep the sets. */
	for (i = beginstatno; i < endstatno; i++) {
		if (slist[i]) free_node(slist[i]);
	}
	free(slist);
#ifdef DEBUG
	if (options['o']) {
		dfl_printgraph(grph);
	}
#endif
	dfl_freegraph(grph);
	set_free(temp_set, tset_sz);
}

static int
	stat_no;

static int compute_defused(nd)
	p_node	nd;
{
	/* Compute DEF and USED in one go. DEF is set for assignments and
	   out-parameters. All other usage is marked as USED.
	*/
	if (nd->nd_class == Stat || nd->nd_type == 0) {
		/* Set stat_no for all statements (and thus also for calls
		   that have no function result. Note that compute_defused
		   is also called for calls that have a result type.
		*/
		stat_no = nd->nd_nodenum;
	}

	switch(nd->nd_symb) {
	case TMPBECOMES:
		assert(nd->nd_desig->nd_class == Tmp);
		(void) mark_def(nd->nd_desig);
		break;
	case BECOMES:
		/* An assignment through a temporary that is a pointer is
		   marked as a use of this temporary, not a def.
		*/
		if (nd->nd_desig->nd_class == Tmp
		    && ! nd->nd_desig->nd_tmpvar->tmp_ispointer) {
			(void) mark_def(nd->nd_desig);
			break;
		}
		/* fall through */
	default:
		if (nd->nd_class == Stat) {
			if (nd->nd_symb == INIT) {
				t_def	*df = nd->nd_desig->nd_def;
				if (df->df_kind == D_OFIELD) {
					visit_ndlist(df->fld_bat, Tmp|Def|Call, mark_used, 0);
				}
				else {
					visit_ndlist(df->var_bat, Tmp|Def|Call, mark_used, 0);
				}
			}
			visit_node(nd->nd_desig, Tmp|Def|Call, mark_used, 0);
		}
		break;
	}
	if (nd->nd_class == Stat) {
		visit_node(nd->nd_expr, Tmp|Def|Call, mark_used, 0);
	}
	else {
		/* A call. Check parameters. */
		p_node	actuals;
		t_dflst	formals;
		t_def	*df;

		if (nd->nd_symb == DOLDOL) {
			visit_node(nd->nd_obj, Tmp|Def|Call, mark_used, 0);
			visit_ndlist(nd->nd_parlist, Tmp|Def|Call, 
					mark_used, 0);
			return 0;
		}
		visit_node(nd->nd_callee, Tmp|Def|Call, mark_used, 0);
		visit_node(nd->nd_obj, Tmp|Def|Call, mark_used, 0);
		actuals = nd->nd_parlist;
		formals = nd->nd_callee->nd_type->prc_params;

		if (nd->nd_callee->nd_type == std_type) {
			if (nd->nd_callee->nd_def->df_stdname == S_READ) {
				node_walklist(actuals, actuals, nd) {
					if (nd->nd_class == Tmp) {
						(void) mark_def(nd);
					}
					else {
						visit_node(nd,
							   Tmp|Def|Call,
							   mark_used,
							   0);
					}
				}
			}
			else {
				node_walklist(actuals, actuals, nd) {
					visit_node(nd,
						   Tmp|Def|Call,
						   mark_used,
						   0);
				}
			}
		}
		else {
			def_walklist(formals, formals, df) {
				nd = node_getlistel(actuals);
				if (is_out_param(df)) {
					if (nd->nd_class == Tmp) {
						(void) mark_def(nd);
					}
					else {
						visit_node(nd,
							   Tmp|Def|Call,
							   mark_used,
							   0);
					}
				}
				else {
					visit_node(nd,
						   Tmp|Def|Call,
						   mark_used,
						   0);
				}
				actuals = node_nextlistel(actuals);
			}
		}
	}
	return 0;
}

static int mark_used(nd)
	p_node	nd;
{
	/*	Called with Def, Tmp, or Call.
		For Call: prevent further descend.
		For Def: mark temporary if it is a FOR-loop variable.
		For Tmp: mark temporary if it is not a return variable.
	*/
	if (nd->nd_class == Call) {
		/* Don't descend further, compute_defused will be called
		   for Calls.
		*/
		return 1;
	}
	if (nd->nd_class == Def) {
		if (nd->nd_def->df_kind == D_VARIABLE &&
		    (nd->nd_def->df_flags & D_FORLOOP)) {
			set_putmember(LV_sets[stat_no].used,
				      nd->nd_def->var_tmpvar->tmp_num);
		}
		return 0;
	}
	if (! (nd->nd_flags & ND_RETVAR)) {
		set_putmember(LV_sets[stat_no].used, nd->nd_tmpvar->tmp_num);
	}
	return 0;
}

static int mark_def(nd)
	p_node	nd;
{
	/*	Called with Tmp or Call.
		For Call: prevent further descend.
		For Tmp: mark temporary if it is not a return variable.
	*/

	if (nd->nd_class == Call) {
		/* Don't descend further, compute_defused will be called
		   for Calls.
		*/
		return 1;
	}
	if (! (nd->nd_flags & ND_RETVAR)) {
		set_putmember(LV_sets[stat_no].def, nd->nd_tmpvar->tmp_num);
	}
	return 0;
}

static int replace_var(nd)
	p_node	nd;
{
	/* Called with Call, Def, or Tmp.
	   Call: handle nd_target.
	   Def:	 handle FOR-loop variables.
	   Tmp:	 map.
	*/
	if (nd->nd_class == Call) {
		visit_node(nd->nd_target, Tmp|Call|Def, replace_var, 0);
		return 0;
	}
	if (nd->nd_class == Def) {
		t_def	*df = nd->nd_def;
		if (df->df_kind == D_VARIABLE &&
		    (df->df_flags & D_FORLOOP) &&
		    var_map[df->var_tmpvar->tmp_num]) {
			df->var_tmpvar =
			   var_tab[var_map[df->var_tmpvar->tmp_num]];
		}
		return 0;
	}
	if (! (nd->nd_flags & ND_RETVAR) && var_map[nd->nd_tmpvar->tmp_num]) {
		nd->nd_tmpvar = var_tab[var_map[nd->nd_tmpvar->tmp_num]];
	}
	return 0;
}

/* Equations for LIVE computation:
   (See Aho, Sethi, Ullman, "Compilers - principles, techniques, and tools",
    march 1988, Algorithm 10.4.)

   out[B] = union over successors S of B of in[S]

   in[B] = union (use[B], out[B] - def[B])
*/

static int live_stat(src, dst)
	int	src, dst;
{
	(void) set_minus(temp_set,
			 LV_sets[src].live,
			 LV_sets[dst].def,
			 tset_sz);
	return set_union(LV_sets[dst].live,
			 LV_sets[dst].used,
			 temp_set,
			 tset_sz);
}

static int live_meet(dst, s)
	int	dst;
	int	*s;
{
	set_init(temp_set, 0, tset_sz);
	while (*s != -1) {
		(void) set_union(temp_set,
				 temp_set,
				 LV_sets[*s].live,
				 tset_sz);
		s++;
	}
	(void) set_minus(temp_set,
			 temp_set,
			 LV_sets[dst].def,
			 tset_sz);
	return set_union(LV_sets[dst].live,
			 LV_sets[dst].used,
			 temp_set,
			 tset_sz);
}
