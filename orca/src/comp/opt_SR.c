/*
 * (c) copyright 1995 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Orca distribution.
 */

/* S T R E N G T H   R E D U C T I O N */

/* $Id: opt_SR.c,v 1.21 1998/03/06 15:48:10 ceriel Exp $ */

#include	"ansi.h"
#include	"debug.h"

#include	<alloc.h>
#include	<assert.h>
#include	<stdio.h>

#include	"opt_SR.h"
#include	"options.h"
#include	"LLlex.h"
#include	"scope.h"
#include	"type.h"
#include	"node_num.h"
#include	"data_flow.h"
#include	"bld_graph.h"
#include	"visit.h"
#include	"oc_stds.h"
#include	"temps.h"
#include	"sets.h"

/* Read "Lazy Strength Reduction" by Jens Knoop, Oliver Ruthing and Bernard
	Steffen, Journal of Programming Languages Vol 1 No 1, pp 71-91, 1993.
   if you want to understand what is going on here.
   This paper presents a number of data flow equations which can be used
   to do strength reduction as well as code motion (and common sub-expression
   elimination).

   We use a slight modification because expressions can be computed more
   than once in a statement: We don't set ISOLATED in this case.
*/

typedef struct SR_info {
	struct fl_graph
		*graph;
	struct SR_sets
		*sets;
	p_node	*exprs;
	p_node	*stats;
	int	*ndeletes;
	int	*nupdates;
	int	*ninserts;
} t_SR_info, *p_SR_info;

struct SR_sets {
	/* Basic expression sets: */
	p_set	used;		/* is expression used (computed) here? */
	p_set	xused;		/* more than once? */
	p_set	trans;		/* does statement affect expression? */
	p_set	SR_trans;	/* or only injure? */
#define injured SR_trans
	/* Expression sets computed through data flow analysis: */
	p_set	dsafe_CM;
#define critins_SR dsafe_CM
#define insert_fstref dsafe_CM
	p_set	insert_CM;
	p_set	earliest_CM;
#define insert_sndref earliest_CM
	p_set	dsafe_SR;
	p_set	delay;
#define subst_crit dsafe_SR
#define delete_sndref dsafe_SR
	p_set	insert_SR;
#define earliest_SR insert_SR
#define isolated insert_SR
	p_set	critical;
#define latest critical
	p_set	insupd_fstref;
};

_PROTOTYPE(static int SR_compute, (p_node));
_PROTOTYPE(static int compute_used, (p_node));
_PROTOTYPE(static int mark_used, (p_node));
_PROTOTYPE(static void eval_trans, (p_node));
_PROTOTYPE(static int use_def, (p_node));
_PROTOTYPE(static int comp_trans, (p_node));
_PROTOTYPE(static void modified, (p_node, int));
_PROTOTYPE(static void modified_num, (int, int));
_PROTOTYPE(static void find_modified_in_call, (p_node));
_PROTOTYPE(static int dsafe_stat, (int, int));
_PROTOTYPE(static int dsafe_meet, (int, int *));
_PROTOTYPE(static int dsafe_SR_stat, (int, int));
_PROTOTYPE(static int dsafe_SR_meet, (int, int *));
_PROTOTYPE(static int earliest_stat, (int, int));
_PROTOTYPE(static int earliest_meet, (int, int *));
_PROTOTYPE(static int earliest_SR_stat, (int, int));
_PROTOTYPE(static int earliest_SR_meet, (int, int *));
_PROTOTYPE(static int critical_stat, (int, int));
_PROTOTYPE(static int critical_meet, (int, int *));
_PROTOTYPE(static int subst_crit_stat, (int, int));
_PROTOTYPE(static int subst_crit_meet, (int, int *));
_PROTOTYPE(static int delay_stat, (int, int));
_PROTOTYPE(static int delay_meet, (int, int *));
_PROTOTYPE(static int isolated_stat, (int, int));
_PROTOTYPE(static int isolated_meet, (int, int *));
_PROTOTYPE(static int insupd_stat, (int, int));
_PROTOTYPE(static int insupd_meet, (int, int *));
_PROTOTYPE(static p_node CM_and_SR, (p_node));
_PROTOTYPE(static void handle_possible_optim, (int, int *, p_node *));
_PROTOTYPE(static void do_update, (int, p_node, p_node, p_node, int, p_node));
_PROTOTYPE(static void do_delete, (p_node, p_node, p_node));
_PROTOTYPE(static int replace_node, (p_node, p_node, p_node, p_node *, int));
_PROTOTYPE(static void insert_new_stats, (p_node *));
_PROTOTYPE(static void insert_list, (p_node *, int, int));
_PROTOTYPE(static int suitable_SR, (p_node));
_PROTOTYPE(static int get_update_expr, (p_node, int, p_node *));
_PROTOTYPE(static int sub_expr, (p_node, p_node, int));
_PROTOTYPE(static void el_modified, (p_node, int));
_PROTOTYPE(static void SR_cleanup, (void));
_PROTOTYPE(static int similar_expr, (int, int, p_node *));
_PROTOTYPE(static void handle_similar_expr, (int, int, p_node));

static t_SR_info
	dflow_sets;
static p_set
	*opt_usages;
static p_set
	candidates;
static t_tmp
	**temporaries;

static int
	eset_sz,		/* Size of expression sets. */
	n_exprs,		/* number of expressions. */
	n_variables,		/* number of variables. */
	endstatno;		/* Highest statement number, reserved
				   for meet of return points.
				*/
#define beginstatno 0

static int
	stat_no;		/* Temporary. */
static p_set
	temp_set, temp_set2,	/* Temporary sets. */
	updates,		/* Set of expressions for which an update
				   is indicated.
				*/
	full_set,		/* Contains all expressions, used for
				   initialization.
				*/
	no_SR;			/* Contains all expressions for which no
				   strength reduction is possible.
				*/

void do_SR(fdf)
	t_def	*fdf;
{
	t_scope	*sc;
	t_def	*df;
	int	n;
	t_scope	*sv_scope = CurrentScope;
	t_tmp	*tmp;


	CurrentScope = fdf->bod_scope;
	n = 1;

	/* Number variables. */
	if (fdf->df_kind == D_OPERATION) {
		/* Object fields ... */
		for (df = fdf->df_scope->sc_def; df; df = df->df_nextinscope) {
			if (df->df_kind == D_OFIELD) {
				df->df_num = n++;
			}
		}
	}
	for (sc = fdf->bod_scope; sc; sc = sc->sc_FOR) {
	    /* Local variables ... */
	    for (df = sc->sc_def; df; df = df->df_nextinscope) {
		if (df->df_kind == D_VARIABLE) {
			df->df_num = n++;
		}
	    }
	}
	for (tmp = fdf->bod_temps; tmp; tmp = tmp->tmp_scopelink) {
		/* and temporaries ... */
		tmp->tmp_num = n++;
	}
	n_variables = n - 1;
	if (fdf->bod_statlist1 && SR_compute(fdf->bod_statlist1)) {
		fdf->bod_statlist1 = CM_and_SR(fdf->bod_statlist1);
		SR_cleanup();
	}
	if (fdf->bod_statlist2 && SR_compute(fdf->bod_statlist2)) {
		fdf->bod_statlist2 = CM_and_SR(fdf->bod_statlist2);
		SR_cleanup();
	}
	CurrentScope = sv_scope;
}

static void
SR_cleanup()
{
	struct SR_sets
		*p;
	int	i;

	for (p = &(dflow_sets.sets[endstatno]); p >= &(dflow_sets.sets[beginstatno]); p--) {
		set_free(p->used, eset_sz);
		set_free(p->xused, eset_sz);
		set_free(p->trans, eset_sz);
		set_free(p->SR_trans, eset_sz);
		set_free(p->dsafe_CM, eset_sz);
		set_free(p->earliest_CM, eset_sz);
		set_free(p->dsafe_SR, eset_sz);
		set_free(p->earliest_SR, eset_sz);
		set_free(p->critical, eset_sz);
		set_free(p->delay, eset_sz);
		set_free(p->insert_CM, eset_sz);
		set_free(p->insupd_fstref, eset_sz);
	}
	free(dflow_sets.sets);

	for (i = beginstatno; i < endstatno; i++) {
		if (dflow_sets.stats[i]) free_node(dflow_sets.stats[i]);
	}
	free(dflow_sets.stats);
	for (i = 1; i < n_exprs; i++) {
		free_node(dflow_sets.exprs[i]);
	}
	free(dflow_sets.exprs);

	dfl_freegraph(dflow_sets.graph);

	for (i = 1; i <= n_variables; i++) {
		set_free(opt_usages[i], eset_sz);
	}
	free(opt_usages);

	set_free(updates, eset_sz);
	set_free(candidates, eset_sz);
	free((void *) temporaries);
}

static int SR_compute(statlist)
	p_node	statlist;
{
	int	i, j;
	struct SR_sets
		*p;
	dfl_graph
		grph;
	p_node	*elist;
	p_node	*slist;

	/* First, number statement nodes and expression nodes, so that
	   we know how many sets of what size to allocate.
	   n_exprs is incremented because statement numbers
	   and expression numbers start at 1, and we would like to use
	   these numbers directly (rather than decrementing them, which is
	   error prone).
	   Also, an extra position is added at begin and end (0 and endstatno).
	   Return 0 if there are no interesting expressions.
	*/
	endstatno = number_statements(statlist, &slist) + 1;
	n_exprs = number_expressions(statlist, &elist) + 1;


	/* Check if 'elist' contains expressions that we are interested in.
	   If not, return immediately.
	*/
	i = 0;
	for (j = 1; j < n_exprs; j++) {
		if (elist[j]->nd_class & (Oper|Uoper|Check|Arrsel|Ofldsel)) i++;

	}
	if (i == 0) {
		for (i = 1; i < n_exprs; i++) {
			free_node(elist[i]);
		}
		free(elist);
		for (i = beginstatno; i < endstatno; i++) {
			if (slist[i]) free_node(slist[i]);
		}
		free(slist);
		return 0;
	}

#ifdef DEBUG
	if (options['o']) {
		printf("================ body ===================\n");
		dump_nodelist(statlist, 2);
	}
#endif

	/* Compute set sizes and allocate. */
	eset_sz = set_size(n_exprs);
	no_SR = set_create(eset_sz);
	set_init(no_SR, 0, eset_sz);
	opt_usages = (p_set *) Malloc((n_variables+1) * sizeof (p_set));
	for (i = 1; i <= n_variables; i++) {
		opt_usages[i] = set_create(eset_sz);
		set_init(opt_usages[i], 0, eset_sz);
	}
	opt_usages[0] = 0;
	temporaries = (t_tmp **) Malloc(n_exprs * sizeof(t_tmp *));
	full_set = set_create(eset_sz);
	set_init(full_set, 1, eset_sz);
	set_clrmember(full_set, 0);
	for (i = n_exprs; i & 07; i++) {
		set_clrmember(full_set, i);
	}
	temp_set = set_create(eset_sz);
	temp_set2 = set_create(eset_sz);
	dflow_sets.sets = (struct SR_sets *)
				Malloc((endstatno+1)*sizeof(struct SR_sets));
	dflow_sets.exprs = elist;
	dflow_sets.stats = slist;

	for (p = &(dflow_sets.sets[endstatno]); p >= &(dflow_sets.sets[beginstatno]); p--) {
		p->used = set_create(eset_sz);
		set_init(p->used, 0, eset_sz);
		p->xused = set_create(eset_sz);
		set_init(p->xused, 0, eset_sz);
		p->trans = set_create(eset_sz);
		set_copy(p->trans, full_set, eset_sz);
		p->SR_trans = set_create(eset_sz);
		set_copy(p->SR_trans, full_set, eset_sz);
		p->dsafe_CM = set_create(eset_sz);
		set_copy(p->dsafe_CM, full_set, eset_sz);
		p->earliest_CM = set_create(eset_sz);
		set_init(p->earliest_CM, 0, eset_sz);
		p->dsafe_SR = set_create(eset_sz);
		set_copy(p->dsafe_SR, full_set, eset_sz);
		p->earliest_SR = set_create(eset_sz);
		set_init(p->earliest_SR, 0, eset_sz);
		p->critical = set_create(eset_sz);
		set_init(p->critical, 0, eset_sz);
		p->delay = set_create(eset_sz);
		p->insert_CM = set_create(eset_sz);
		set_init(p->insert_CM, 0, eset_sz);
		p->insupd_fstref = set_create(eset_sz);
		set_init(p->insupd_fstref, 0, eset_sz);
	}

	for (j = 1; j < n_exprs; j++) {
		temporaries[j] = 0;
		if (! suitable_SR(elist[j])) {
			set_putmember(no_SR, j);
		}
	}

	/* Now, compute the flow graph. */
	grph = dfl_buildgraph(endstatno+1, beginstatno, statlist);
	dflow_sets.graph = grph;

	/* Then, compute local predicates.  */

	/* First: USED: set when expression is used (evaluated) in
	   a statement.
	*/
	visit_ndlist(statlist, Stat|Call, compute_used, 0);

	/* Next: TRANS: set when an expression is transparent for a
	   statement, t.i. evaluation of the statement does not modify the
	   value of the expression.
	   Also: SR_TRANS, which is TRANS or an update of a loop variable.
	*/
	eval_trans(statlist);

	/* Compute dsafe_CM and dsafe_SR. Greatest solution is required,
	   so all sets are initialized to full. However, the equation
	   specifies that the end set is empty.
	*/
	set_init(dflow_sets.sets[endstatno].dsafe_CM, 0, eset_sz);
	set_init(dflow_sets.sets[endstatno].dsafe_SR, 0, eset_sz);
	dfl_backward_flow(grph, dsafe_meet, dsafe_stat);
	dfl_backward_flow(grph, dsafe_SR_meet, dsafe_SR_stat);

	/* Compute earliest_CM and earliest_SR. Least solution is required,
	   so all sets are initialized to empty. However, the equation
	   specifies that the begin set is full.
	*/
	set_copy(dflow_sets.sets[beginstatno].earliest_CM, full_set, eset_sz);
	set_copy(dflow_sets.sets[beginstatno].earliest_SR, full_set, eset_sz);
	dfl_forward_flow(grph, earliest_meet, earliest_stat);
	dfl_forward_flow(grph, earliest_SR_meet, earliest_SR_stat);

#ifdef DEBUG
	if (options['o']) {
		printf("graph: \n");
		dfl_printgraph(grph);
	}
	if (options['o'] > 1) {
		int i, j;
		for (i = 1; i < n_exprs; i++) {
			printf("expression %d:\n", i);
			dump_node(elist[i], 2);
			for (j = beginstatno; j <= endstatno; j++) {
				printf("    statement %d", j);
				p = &(dflow_sets.sets[j]);
				if (set_ismember(p->used, i)) {
					printf(", USED");
				}
				if (set_ismember(p->trans, i)) {
					printf(", TRANS");
				}
				if (set_ismember(p->SR_trans, i)) {
					printf(", SR_TRANS");
				}
				if (set_ismember(p->dsafe_CM, i)) {
					printf(", DSAFE_CM");
				}
				if (set_ismember(p->dsafe_SR, i)) {
					printf(", DSAFE_SR");
				}
				if (set_ismember(p->earliest_CM, i)) {
					printf(", EARLIEST_CM");
				}
				if (set_ismember(p->earliest_SR, i)) {
					printf(", EARLIEST_SR");
				}
				printf("\n");
			}
		}
	}
#endif

	/* Compute insert_CM and insert_SR. */
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		(void) set_intersect(p->insert_CM,
				     p->earliest_CM,
				     p->dsafe_CM,
				     eset_sz);
		(void) set_intersect(p->insert_SR,
				     p->earliest_SR,
				     p->dsafe_SR,
				     eset_sz);
	}

	set_free(no_SR, eset_sz);

	/* Compute critical. */
	dfl_backward_flow(grph, critical_meet, critical_stat);

	/* Compute CritInsSR, which is needed for subst_crit. */
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		(void) set_intersect(p->critins_SR,
				     p->critical,
				     p->insert_SR,
				     eset_sz);
		set_init(p->subst_crit, 0, eset_sz);
	}

	/* Compute subst_crit. */
	dfl_forward_flow(grph, subst_crit_meet, subst_crit_stat);

	/* Compute insert_fstref. */
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		(void) set_intersect(p->subst_crit,
				     p->subst_crit,
				     p->insert_CM,
				     eset_sz);
		(void) set_Xintersect(p->critical,
				     p->critical,
				     p->insert_SR,
				     eset_sz);
		(void) set_union(p->insert_fstref,
				 p->critical,
				 p->subst_crit,
				 eset_sz);
		set_copy(p->delay, full_set, eset_sz);
	}

	/* Compute delay and insupd_fstref. */
	set_copy(dflow_sets.sets[beginstatno].delay,
		 dflow_sets.sets[beginstatno].insert_fstref,
		 eset_sz);
	dfl_forward_flow(grph, delay_meet, delay_stat);
	dfl_backward_flow(grph, insupd_meet, insupd_stat);

	/* Compute latest. */
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		int i;

		set_copy(temp_set, full_set, eset_sz);
		for (i = beginstatno; i <= endstatno; i++) {
			if (dfl_issuccessor(grph,
					    p - dflow_sets.sets,
					    i)) {
				(void) set_intersect(
					temp_set,
					temp_set,
					dflow_sets.sets[i].delay,
					eset_sz);
			}
		}
		(void) set_Xunion(temp_set, temp_set, p->used, eset_sz);
		(void) set_intersect(p->latest, p->delay, temp_set, eset_sz);
	}

	/* We still have to intersect insupd_fstref with Injured,
	   which is SR_trans - trans. Also compute updates.
	*/
	updates = set_create(eset_sz);
	set_init(updates, 0, eset_sz);
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		(void) set_minus(p->injured,
				 p->SR_trans,
				 p->trans,
				 eset_sz);
		(void) set_intersect(p->insupd_fstref,
				     p->insupd_fstref,
				     p->injured,
				     eset_sz);
		(void) set_union(updates,
				 updates,
				 p->insupd_fstref,
				 eset_sz);
		set_copy(p->isolated, full_set, eset_sz);
	}

	/* Compute isolated. */
	dfl_backward_flow(grph, isolated_meet, isolated_stat);

	/* Own adjust: delete statements that compute expression more than once
	   from ISOLATED.
	*/
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		(void) set_minus(p->isolated, p->isolated, p->xused, eset_sz);
	}

	/* Compute insert_sndref and delete_sndref. insupd_sndref is equal
	   to insupd_fstref. Also, compute the set of expressions that
	   may get a temporary, and store it in candidates.
	*/
	candidates = set_create(eset_sz);
	set_init(candidates, 0, eset_sz);
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		(void) set_intersect(p->delete_sndref,
				     p->isolated,
				     p->latest,
				     eset_sz);
		(void) set_Xintersect(p->delete_sndref,
				      p->delete_sndref,
				      p->used,
				      eset_sz);
		(void) set_Xintersect(p->insert_sndref,
				      p->isolated,
				      p->latest,
				      eset_sz);
		set_union(candidates, candidates, p->insert_sndref, eset_sz);
	}

#ifdef DEBUG
	if (options['o']) {
		int i, j;

		for (i = 1; i < n_exprs; i++) {
			printf("expression %d", i);
			if (set_ismember(candidates, i)) {
				printf("(may get temporary)");
			}
			printf(":\n");
			dump_node(elist[i], 2);
			for (j = beginstatno; j <= endstatno; j++) {
				printf("    statement %d", j);
				p = &(dflow_sets.sets[j]);
				if (set_ismember(p->delay, i)) {
					printf(", DELAY");
				}
				if (set_ismember(p->latest, i)) {
					printf(", LATEST");
				}
				if (set_ismember(p->isolated, i)) {
					printf(", ISOLATED");
				}
				if (set_ismember(p->insert_fstref, i)) {
					printf(", INSERT_FSTREF");
				}
				if (set_ismember(p->insupd_fstref, i)) {
					printf(", INSUPD_FSTREF");
				}
				if (set_ismember(p->insert_sndref, i)) {
					printf(", INSERT_SNDREF");
				}
				if (set_ismember(p->delete_sndref, i)) {
					printf(", DELETE_SNDREF");
				}
				printf("\n");
			}
		}
	}
#endif
	set_free(temp_set, eset_sz);
	set_free(temp_set2, eset_sz);
	set_free(full_set, eset_sz);
	return 1;
}

static int
suitable_SR(nd)
	p_node	nd;
{
	/*	Find out if the expression indicated by nd is suitable
		for strength reduction.
		If i is a loop variable, the following types of expressions
		are suitable:

			b * i
			c + i
			b * i + c
			a[i]
			a[c+i]
			a[b*i+c]

		This routine merely acts as a kind of filter, before the
		actual computation is done. We still may do code motion
		on expressions that are rejected by this function, and this
		is not possible if the mechanism thinks that it can do
		strength reduction, so we have to know in advance for which
		expressions we can not do it.
	*/

	static int
		a;
	int	l1, l2;
	int	retval = 0;
	static int
		level;

	if (! level) a = 0;

	level++;
	switch(nd->nd_class) {
	case Select:
		if (level != 1) retval = suitable_SR(nd->nd_left);
		break;

	case Arrsel:
		l1 = suitable_SR(nd->nd_left);
		if (l1) {
			a = 0;
			l2 = suitable_SR(nd->nd_right);
			if (! a && (l2 > l1 || (l1 == 1 && l2 == 1))) {
				a = Arrsel;
				retval = l2;
			}
		}
		break;

	case Check:
		if (nd->nd_symb == A_CHECK) {
			l1 = nd->nd_left ? suitable_SR(nd->nd_left) : 1;
			if (l1) {
				a = 0;
				l2 = suitable_SR(nd->nd_right);
				if (! a && (l2 > l1 || (l1 == 1 && l2 == 1))) {
					retval = l2;
				}
			}
		}
		break;

	case Uoper:
		if (nd->nd_symb == ARR_SIZE) {
			if (! nd->nd_right ||
			    suitable_SR(nd->nd_right) == 1) retval = 1;
			break;
		}
		if (nd->nd_symb != '-') break;
		switch((l1 = suitable_SR(nd->nd_right))) {
		case 0:
			break;
		case 1:
			retval = 1;
			break;
		default:
			if (a) break;
			retval = l1;
		}
		break;

	case Tmp:
		if (nd->nd_tmpvar->tmp_expr) {
			retval = suitable_SR(nd->nd_tmpvar->tmp_expr);
		}
		break;

	case Def:
		if (nd->nd_def->df_flags & D_FORLOOP) {
			retval = nd->nd_def->var_level + 1;
		}
		else retval = 1;
		break;

	case Value:
		retval = 1;
		break;

	case Oper:
		switch(nd->nd_symb) {
		case '+':
		case '-':
		case '*':
			l1 = suitable_SR(nd->nd_left);
			if (! l1) break;
			l2 = suitable_SR(nd->nd_right);
			if (! l2) break;
			/* if (l1 > 1 && l2 > 1) break; */
			if (l1 == 1 && l2 == 1) retval = 1;
			else {
				if (l1 == l2) break;
				if (a) break;
				if (l1 > l2) retval = l1;
				else retval = l2;
			}
		}
		break;
	}
	level--;
	return retval;
}

static int compute_used(nd)
	p_node	nd;
{
	/* Computes the set of expressions used in the statement indicated
	   by nd, by setting stat_no to its statement number and then
	   visiting all expression nodes through visit_node and mark_used.
	   Also invokes compute_used for nested statements.
	*/
	if (nd->nd_class != Call || ! nd->nd_type) {
		/* Statement level. Note that compute_used is also called for
		   Call nodes in expressions, and we don't want to set
		   stat_no for those.
		*/
		stat_no = nd->nd_nodenum;
	}

	switch(nd->nd_symb) {
	case ORBECOMES:
	case ANDBECOMES:
		visit_node(nd->nd_expr, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
		visit_node(nd->nd_desig, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
		visit_ndlist(nd->nd_list1, Call|Stat, compute_used, 0);
		break;

	case IF:
	case FOR:
	case CASE:
		visit_node(nd->nd_expr, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
		/* Fall through */
	case DO:
		visit_ndlist(nd->nd_list1, Call|Stat, compute_used, 0);
		/* Fall through */
	case ARROW:
		visit_ndlist(nd->nd_list2, Call|Stat, compute_used, 0);
		break;
	case GUARD:
		visit_ndlist(nd->nd_list1, Call|Stat, compute_used, 0);
		break;
	case FOR_UPDATE:
		break;
	default:
		if (nd->nd_class == Stat) {
			visit_node(nd->nd_expr, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
			visit_node(nd->nd_desig, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
		}
		else {
			visit_node(nd->nd_callee, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
			visit_node(nd->nd_obj, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
			visit_ndlist(nd->nd_parlist, Oper|Uoper|Arrsel|Ofldsel|Select|Check|Call, mark_used, 1);
		}
		break;
	}
	return 1;
}

static int mark_used(nd)
	p_node	nd;
{
	/* Record the fact that statement stat_no uses the expression
	   indicated by nd.
	*/

	if (nd->nd_class == Call && ! nd->nd_type) return 0;

	assert((nd->nd_nodenum >> 3) < eset_sz);
	if (set_ismember(dflow_sets.sets[stat_no].used, nd->nd_nodenum)) {
		set_putmember(dflow_sets.sets[stat_no].xused, nd->nd_nodenum);
	}
	else	set_putmember(dflow_sets.sets[stat_no].used, nd->nd_nodenum);
	return 0;
}

static void eval_trans(statlist)
	p_node	statlist;
{
	/* Determining TRANS is done as follows:
	   First, for every variable V, the set of expressions in which it
	   is used is determined. Call this set U(V). Then, for every statement
	   S: TRANS = ~(union over all V changed in S of U(V)).
	*/
	struct SR_sets
		*p;

	/* Find out in which expressions each variable is used. */
	set_init(temp_set, 0, eset_sz);
	visit_ndlist(statlist,
		     Def|Arrsel|Ofldsel|Oper|Uoper|Select|Check|Tmp|Call,
		     use_def,
		     0);

	/* Next, visit each statement, determining which variables are
	   changed, and computing TRANS and SR_TRANS.
	*/
	visit_ndlist(statlist, Call|Stat, comp_trans, 0);

	/* Modify SR_trans for expressions that we don't do strength reduction
	   on.
	*/
	for (p = &(dflow_sets.sets[endstatno]);
	     p >= &(dflow_sets.sets[beginstatno]);
	     p--) {
		int	j;

		for (j = 1; j < n_exprs; j++) {
			if ( set_ismember(no_SR, j) &&
			     set_ismember(p->SR_trans, j) &&
			     ! set_ismember(p->trans, j)) {
				set_clrmember(p->SR_trans, j);
			}
		}
	}
}

static int
use_def(nd)
	p_node	nd;
{
	/* If nd indicates an expression, add it to the set of current
	   expressions and walk its subtree so that all variables used
	   in the subtree can be marked as being used in this expression.
	   If nd indicates a variable, add the set of current expressions to
	   the set of expressions that use this variable.
	*/
	p_set	df_optim;

	switch(nd->nd_class) {
	case Ofldsel:
		assert(nd->nd_nodenum >= 0 && (nd->nd_nodenum >> 3) < eset_sz);
		if (CurrentScope->sc_definedby->df_flags & D_PARALLEL) {
		    /* Don't mark the LHS as used. It cannot be overwritten
		       in a parallel operation, and marking it as used may
		       block some optimizations when assigments are done
		       on the one object field which IS overwritten.
		    */
		    set_putmember(temp_set, nd->nd_nodenum);
		    visit_node(nd->nd_right,
			   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
			   use_def,
			   0);
		    set_clrmember(temp_set, nd->nd_nodenum);
		    break;
		}
		/* Fall through */
	case Arrsel:
	case Select:
	case Oper:
	case Check:
		assert(nd->nd_nodenum >= 0 && (nd->nd_nodenum >> 3) < eset_sz);
		set_putmember(temp_set, nd->nd_nodenum);
		visit_node(nd->nd_left,
			   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
			   use_def,
			   0);
		visit_node(nd->nd_right,
			   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
			   use_def,
			   0);
		set_clrmember(temp_set, nd->nd_nodenum);
		break;
	case Uoper:
		assert(nd->nd_nodenum >= 0 && (nd->nd_nodenum >> 3) < eset_sz);
		set_putmember(temp_set, nd->nd_nodenum);
		visit_node(nd->nd_right,
			   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
			   use_def,
			   0);
		set_clrmember(temp_set, nd->nd_nodenum);
		break;
	case Tmp:
		if (nd->nd_tmpvar->tmp_expr) {
			visit_node(nd->nd_tmpvar->tmp_expr,
				   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
				   use_def,
				   0);
		}
		if (nd->nd_tmpvar) {
			df_optim = opt_usages[nd->nd_tmpvar->tmp_num];
			if (df_optim) {
				(void) set_union(df_optim,
						 df_optim,
						 temp_set,
						 eset_sz);
			}
		}
		break;
	case Call:
		if (! nd->nd_type) return 0;
		set_putmember(temp_set, nd->nd_nodenum);
		visit_node(nd->nd_callee,
			   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
			   use_def,
			   0);
		visit_node(nd->nd_obj,
			   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
			   use_def,
			   0);
		visit_ndlist(nd->nd_parlist,
			   Call|Def|Arrsel|Ofldsel|Oper|Uoper|Select|Tmp,
			   use_def,
			   0);
		set_clrmember(temp_set, nd->nd_nodenum);
		break;

	case Def:
		if (nd->nd_def->df_kind & (D_VARIABLE|D_OFIELD)) {
			df_optim = opt_usages[nd->nd_def->df_num];
			if (df_optim) {
				(void) set_union(df_optim,
						 df_optim,
						 temp_set,
						 eset_sz);
			}
		}
		break;
	}
	return 1;
}

static void
modified_num(num, xtrans)
	int	num;
	int	xtrans;
{
	if (! opt_usages[num]) return;
	(void) set_minus(dflow_sets.sets[stat_no].trans,
			 dflow_sets.sets[stat_no].trans,
			 opt_usages[num],
			 eset_sz);
	if (! xtrans) {
		(void) set_minus(dflow_sets.sets[stat_no].SR_trans,
				 dflow_sets.sets[stat_no].SR_trans,
				 opt_usages[num],
				 eset_sz);
	}
}

static void
modified(nd, xtrans)
	p_node	nd;
	int	xtrans;
{
	/* The designator indicated by nd is modified. If xtrans is set,
	   it is an update of a FOR-loop. This routine marks the effect
	   on statement stat_no.
	*/
	switch(nd->nd_class) {
	case Def:
		if (nd->nd_def->df_flags & D_SELF) {
			t_def	*df = enclosing(CurrentScope)->sc_def;

			for (; df; df = df->df_nextinscope) {
				if (df->df_kind == D_OFIELD) {
					modified_num(df->df_num, xtrans);
				}
			}
			break;
		}
		if (nd->nd_def->df_kind & (D_VARIABLE|D_OFIELD)) {
			modified_num(nd->nd_def->df_num, xtrans);
		}
		break;
	case Tmp:
		if (nd->nd_tmpvar) {
			modified_num(nd->nd_tmpvar->tmp_num, xtrans);
		}
		break;
	case Select:
	case Arrsel:
	case Ofldsel:
		el_modified(nd, xtrans);
		break;
	}
}

static void el_modified(nd, xtrans)
	p_node	nd;
	int	xtrans;
{
	/*	The array element / graph node / field indicated by nd is
		changed.  Examine effects.
	*/
	int	i;
	int	num = 0;

	if ((nd->nd_class & (Select|Ofldsel)) &&
	    (nd->nd_left->nd_class & (Def | Tmp))) {
		num = nd->nd_left->nd_class == Def ?
				nd->nd_left->nd_def->df_num :
				nd->nd_left->nd_tmpvar->tmp_num;
	}

	for (i = 1; i < n_exprs; i++) {
		if (num && ! set_ismember(opt_usages[num], i)) {
			continue;
		}

		if (nd->nd_class == Arrsel &&
		    dflow_sets.exprs[i]->nd_class == Arrsel &&
		    nd->nd_type == dflow_sets.exprs[i]->nd_type) {
			continue;
		}

		if (sub_expr(nd, dflow_sets.exprs[i], num == 0)) {
			set_clrmember(dflow_sets.sets[stat_no].trans, i);
			if (! xtrans) {
				set_clrmember(dflow_sets.sets[stat_no].SR_trans, i);
			}
		}
	}
}

static int
comp_trans(nd)
	p_node	nd;
{
	/* Compute trans and SR_trans for the current statement.
	   This is done by subtracting the set of all expressions that use
	   any variable that is changed in the current statement from the
	   full set of expressions.
	*/
	if (nd->nd_class != Call || ! nd->nd_type) {
		/* Statement level. Note that comp_trans is also called for
		   Call nodes in expressions, and we don't want to set
		   stat_no for those.
		*/
		stat_no = nd->nd_nodenum;
	}

	/* Find all modified variables. */
	switch(nd->nd_class) {
	case Call:
		find_modified_in_call(nd);
		break;
	case Stat:
		switch(nd->nd_symb) {
		case BECOMES:
		case PLUSBECOMES:
		case MINBECOMES:
		case TIMESBECOMES:
		case DIVBECOMES:
		case MODBECOMES:
		case ORBECOMES:
		case B_ORBECOMES:
		case B_XORBECOMES:
		case ANDBECOMES:
		case B_ANDBECOMES:
		case LSHBECOMES:
		case RSHBECOMES:
		case TMPBECOMES:
		case FOR:
			modified(nd->nd_desig, 0);
			break;

		case FOR_UPDATE:
			if (nd->nd_expr->nd_class == Link) {
				modified(nd->nd_desig, 1);
			}
			else modified(nd->nd_desig, 0);
			break;
		}
		break;
	}
	return 0;
}

static void find_modified_in_call(nd)
	p_node	nd;
{
	/* Find all modified designators in a call, and call "modified" for
	   each of them. In a call, modified designators are SHARED or OUT
	   parameters, and objects on which a write operation is done.
	*/

	t_dflst	formals;
	p_node	actuals;
	t_def	*df;

	if (nd->nd_symb == DOLDOL) return;
	actuals = nd->nd_parlist;
	formals = nd->nd_callee->nd_type->prc_params;
	if (nd->nd_symb == '$') {
		/* Operation. Check if it has write alternatives, and if so,
		   note the object designator as modified.
		*/
		if (nd->nd_callee->nd_def->df_flags & D_HASWRITES) {
			modified(nd->nd_obj, 0);
		}
	}

	if (nd->nd_callee->nd_type == std_type) {
		/* The call is an Orca built-in. Some of these change
		   variables.
		*/
		switch(nd->nd_callee->nd_def->df_stdname) {
		case S_FROM:
		case S_DELETENODE:
			/* First parameter is affected. */
			modified(node_getlistel(actuals), 0);
			break;
		case S_INSERT:
		case S_DELETE:
			/* Second parameter is affected. */
			modified(node_getlistel(node_nextlistel(actuals)), 0);
			break;
		case S_READ:
			/* All parameters are affected. */
			node_walklist(actuals, actuals, nd) {
				modified(nd, 0);
			}
			break;
		}
		return;
	}

	/* Now, we have an ordinary non-built-in call. */
	def_walklist(formals, formals, df) {
		if (is_out_param(df) || is_shared_param(df)) {
			modified(node_getlistel(actuals), 0);
		}
		actuals = node_nextlistel(actuals);
	}
}

static p_node	*new_stats;

static int
get_update_expr(nd, exprnum, factor)
	p_node	nd;
	int	exprnum;
	p_node	*factor;
{
	/*	Find out if the expression indicated by exprnum is suitable
		for strength reduction. This routine is also used for
		sub-expressions of this expression, and these are indicated
		by nd. This function is similar to suitable_SR, but more
		precise, because it also has the insupd sets available.
		It also computes the update expressions if strength
		reduction is to be applied, and stores it in factor.
	*/

	int	j;
	static int
		a;

	if (nd->nd_nodenum == exprnum) {
		a = 0;
		*factor = 0;
	}

	/* if (! set_ismember(updates, exprnum)) return 1; */

	switch(nd->nd_class) {
	case Select:
		if (nd->nd_nodenum == exprnum) break;
		switch(get_update_expr(nd->nd_left, exprnum, factor)) {
		case 1:
			return 1;
		}
		break;

	case Arrsel:
		if (get_update_expr(nd->nd_left, exprnum, factor) == 1) {
			a = 0;
			switch (get_update_expr(nd->nd_right, exprnum, factor)) {
			case 2:
				if (a) break;
				a = Arrsel;
				return 2;
			case 1:
				return 1;
			}
		}
		break;

	case Check:
		if (nd->nd_symb == A_CHECK) {
			if (! nd->nd_left ||
			    get_update_expr(nd->nd_left,exprnum, factor) == 1) {
				a = 0;
				switch (get_update_expr(nd->nd_right, exprnum, factor)) {
				case 1:
					return 1;
				case 2:
					if (a != 0) break;
					return 2;
				}
			}
		}
		break;

	case Uoper:
		if (nd->nd_symb == ARR_SIZE) {
			if (! nd->nd_right) return 1;
			switch (get_update_expr(nd->nd_right, exprnum, factor)) {
			case 0:
			case 2:
				break;
			case 1:
				return 1;
			}
		}
		if (nd->nd_symb != '-') break;
		switch(get_update_expr(nd->nd_right, exprnum, factor)) {
		case 0:
			break;
		case 1:
			return 1;
		case 2: {
			if (a) break;
			if (*factor) {
				if ((*factor)->nd_class == Value) {
					(*factor)->nd_int = - (*factor)->nd_int;
				}
				else {
					p_node	n = mk_leaf(Uoper, '-');
					n->nd_right = (*factor);
					n->nd_type = (*factor)->nd_type;
					n->nd_pos = (*factor)->nd_pos;
					(*factor) = n;
				}
			}
			else {
				(*factor) = mk_leaf(Value, INTEGER);
				(*factor)->nd_int = -1;
				(*factor)->nd_type = int_type;
				(*factor)->nd_pos = nd->nd_pos;
			}
			return 2;
			}
		}
		break;

	case Tmp:
		if (nd->nd_tmpvar->tmp_expr) {
			return get_update_expr(nd->nd_tmpvar->tmp_expr, exprnum, factor);
		}
		break;

	case Def:
		if (nd->nd_def->df_flags & D_FORLOOP) {
			for (j = beginstatno; j <= endstatno; j++) {
			    if (set_ismember(dflow_sets.sets[j].insupd_fstref,
					     exprnum) &&
				dflow_sets.stats[j]->nd_desig->nd_def ==
					nd->nd_def) {
				return 2;
			    }
			}
		}
		return 1;

	case Value:
		return 1;

	case Oper:
		switch(nd->nd_symb) {
		case '+':
		case '-':
		case '*':
			{
			int	l = get_update_expr(nd->nd_left, exprnum, factor);
			int	r;
			p_node	n;

			if (! l) break;
			r = get_update_expr(nd->nd_right, exprnum, factor);
			if (! r) break;
			n = *factor;
			switch(l + r) {
			case 2:
				return 1;
			case 3:
				if (a) break;
				if (nd->nd_symb == '*') {
					p_node	cp =
						node_copy(l == 2 ?
							    nd->nd_right :
							    nd->nd_left);
					if (n) {
						n = mk_expr(Oper, '*', n, cp);
						n->nd_type = nd->nd_type;
						n->nd_pos = nd->nd_pos;
					}
					else {
						n = cp;
					}
				}
				else if (nd->nd_symb == '-' && r == 2) {
					if (! n) {
						n = mk_leaf(Value, INTEGER);
						n->nd_int = -1;
					}
					else {
						n = mk_expr(
							Uoper,
							'-',
							(p_node) 0,
							n);
					}
					n->nd_type = nd->nd_type;
					n->nd_pos = nd->nd_pos;
				}
				*factor = n;
				return 2;
			}
			}
			break;
		}
		break;
	}
	if ((*factor)) {
		kill_node((*factor));
		(*factor) = 0;
	}
	return 0;
}

int
suitable_CMSR(nd)
	p_node	nd;
{
	/*	Check if the expression indicated by 'nd' is suitable
		for Code Motion and/or Strength Reduction.
		Return -1 if it may not be moved at all,
			0 if it may be moved but no gain is expected,
			1 otherwise.
	*/
	int	retval = -1;

	if (! nd) return 0;
	if (nd->nd_flags & ND_SUITABLE_SR) return 1;
	if (nd->nd_flags & ND_NO_SR) return -1;
	switch(nd->nd_class) {
	case Call:
		if (nd->nd_symb == '(' &&
		    nd->nd_callee->nd_type == std_type) {
			p_node	n;
			switch(nd->nd_callee->nd_def->df_stdname) {
			case S_MYCPU:
			case S_NCPUS:
				retval = 1;
				break;
			case S_CAP:
			case S_CHR:
			case S_ODD:
			case S_SIZE:
			case S_ORD:
			case S_FLOAT:
			case S_TRUNC:
				if (suitable_CMSR(node_getlistel(nd->nd_parlist)) != -1) {
					retval = 1;
				}
				break;
			case S_VAL:
				n = node_getlistel(node_nextlistel(nd->nd_parlist));
				if (suitable_CMSR(n) != -1) {
					retval = 1;
				}
				break;
			case S_LB:
			case S_UB:
				{
				p_node	n1 = node_getlistel(nd->nd_parlist);
				n = node_getlistel(node_nextlistel(nd->nd_parlist));
				if (suitable_CMSR(n1) != -1 &&
				    (! n || suitable_CMSR(n) != -1)) {
					retval = 1;
				}
				}
				break;
			}
		}
		break;
	case Select:
		if (suitable_CMSR(nd->nd_left) != -1) {
			retval = 0;
		}
		break;
	case Oper:
	case Arrsel:
	case Check:
	case Ofldsel:
		switch(nd->nd_symb) {
		case AND:
		case OR:
		case '<':
		case '>':
		case '=':
		case NOTEQUAL:
		case LESSEQUAL:
		case GREATEREQUAL:
			break;
		default:
		    if ((! nd->nd_left || suitable_CMSR(nd->nd_left) != -1) &&
			suitable_CMSR(nd->nd_right) != -1) {
			retval = 1;
		    }
		}
		break;
	case Uoper:
		if (! nd->nd_right || suitable_CMSR(nd->nd_right) != -1) {
			retval = 1;
		}
		break;
	default:
		retval = 0;
		break;
	}
	if (retval == -1) nd->nd_flags |= ND_NO_SR;
	else if (retval == 1) nd->nd_flags |= ND_SUITABLE_SR;
	return retval;
}

static int
sub_expr(e1, e2, ignore_index)
	p_node	e1,
		e2;
	int	ignore_index;
{
	/* Return 1 if e1 is a possible sub-expression of e2.
	   If ignore_index is set, possible array or graph indeces are
	   ignored in the comparison.
	*/

	if (! e2) return 0;
	if (e1->nd_nodenum && e1->nd_nodenum == e2->nd_nodenum) return 1;
	switch(e2->nd_class) {
	case Tmp:
		if (e1->nd_tmpvar == e2->nd_tmpvar) return 1;
		break;
	case Def:
		if (e1->nd_def == e2->nd_def) return 1;
		break;
	case Value:
		if (e1->nd_type == e2->nd_type) {
			if (e1->nd_type->tp_fund & T_DISCRETE) {
				return e1->nd_int == e2->nd_int;
			}
			if (e1->nd_type->tp_fund == T_REAL) {
				return e1->nd_real == e2->nd_real;
			}
		}
		break;
	case Link:
	case Oper:
	case Check:
		if (sub_expr(e1, e2->nd_left, ignore_index)) return 1;
		if (sub_expr(e1, e2->nd_right, ignore_index)) return 1;
		break;
	case Ofldsel:
	case Arrsel:
		if (ignore_index &&
		    e1->nd_class == Arrsel &&
		    e1->nd_left->nd_type == e2->nd_left->nd_type) {
			if (sub_expr(e1->nd_left, e2->nd_left, 1)) return 1;
		}
		if (sub_expr(e1, e2->nd_left, ignore_index)) return 1;
		if (sub_expr(e1, e2->nd_right, ignore_index)) return 1;
		break;
	case Uoper:
		if (sub_expr(e1, e2->nd_right, ignore_index)) return 1;
		break;
	case Select:
		if (ignore_index &&
		    e1->nd_class == Select &&
		    e1->nd_right->nd_def == e2->nd_right->nd_def) {
			return sub_expr(e1->nd_left, e2->nd_left, 1);
		}
		if (sub_expr(e1, e2->nd_left, ignore_index)) return 1;
		break;
	case Call: {
		p_node	l;
		if (sub_expr(e1, e2->nd_callee, ignore_index)) return 1;
		node_walklist(e2->nd_parlist, l, e2) {
			if (sub_expr(e1, e2, ignore_index)) return 1;
		}
		}
		break;
	case Aggr:
	case Row: {
		p_node	l;
		node_walklist(e2->nd_memlist, l, e2) {
			if (sub_expr(e1, e2, ignore_index)) return 1;
		}
		}
		break;
	}
	return 0;
}

static p_node
CM_and_SR(statlist)
	p_node	statlist;
{
	int	i;
	int	sset_sz = set_size(endstatno+1);
	p_set	*inserts, *deletes, *ins_updates;
	p_node	*update_exprs;
	int	*may_update;

	inittemps();

	new_stats = (p_node *) Malloc((endstatno+1) * sizeof(p_node));
	for (i = beginstatno; i <= endstatno; i++) {
		node_initlist(&new_stats[i]);
	}

	dflow_sets.ndeletes = (int *) Malloc(sizeof(int) * n_exprs);
	dflow_sets.nupdates = (int *) Malloc(sizeof(int) * n_exprs);
	dflow_sets.ninserts = (int *) Malloc(sizeof(int) * n_exprs);
	inserts = (p_set *) Malloc(n_exprs * sizeof(p_set));
	deletes = (p_set *) Malloc(n_exprs * sizeof(p_set));
	ins_updates = (p_set *) Malloc(n_exprs * sizeof(p_set));
	update_exprs = (p_node *) Malloc(n_exprs * sizeof(p_node));
	may_update = (int *) Malloc(n_exprs * sizeof(int));

	for (i = 1; i < n_exprs; i++) {
		int	j;
		struct SR_sets
			*p = dflow_sets.sets;

		dflow_sets.nupdates[i] = 0;
		dflow_sets.ndeletes[i] = 0;
		dflow_sets.ninserts[i] = 0;
		inserts[i] = set_create(sset_sz);
		deletes[i] = set_create(sset_sz);
		ins_updates[i] = set_create(sset_sz);
		set_init(inserts[i], 0, sset_sz);
		set_init(deletes[i], 0, sset_sz);
		set_init(ins_updates[i], 0, sset_sz);
		for (j = beginstatno; j <= endstatno; p++, j++) {
			if (set_ismember(p->insert_sndref, i)) {
				dflow_sets.ninserts[i]++;
				set_putmember(inserts[i], j);
			}
			if (set_ismember(p->delete_sndref, i)) {
				dflow_sets.ndeletes[i]++;
				set_putmember(deletes[i], j);
			}
			if (set_ismember(p->insupd_fstref, i)) {
				dflow_sets.nupdates[i]++;
				set_putmember(ins_updates[i], j);
			}
		}
		if (set_ismember(candidates, i)) {
			p_node	nd = dflow_sets.exprs[i];
			if (suitable_CMSR(nd) <= 0) {
				set_clrmember(candidates, i);
			}
			switch(nd->nd_class) {
			case Oper:
				if ((nd->nd_symb == '+' || nd->nd_symb == '-') &&
				    ((nd->nd_left->nd_class == Value &&
				      (nd->nd_right->nd_class & (Tmp|Def))) ||
				     (nd->nd_right->nd_class == Value &&
				      (nd->nd_left->nd_class & (Tmp|Def)))) &&
				    (dflow_sets.nupdates[nd->nd_nodenum] +
				     dflow_sets.ninserts[nd->nd_nodenum] >=
				     dflow_sets.ndeletes[nd->nd_nodenum]-1)) {
					set_clrmember(candidates, i);
				}
				break;
			case Uoper:
				if (nd->nd_symb == '-' &&
				    (nd->nd_right->nd_class & (Tmp | Def)) &&
				    (dflow_sets.nupdates[nd->nd_nodenum] +
				     dflow_sets.ninserts[nd->nd_nodenum] >=
				     dflow_sets.ndeletes[nd->nd_nodenum]-1)) {
					set_clrmember(candidates, i);
				}
				break;
			}
		}
	}

	/* See if the expression is a sub-expression of
	   another expression that will be moved/eliminated.
	   We do this by first allocating and computing for each expression
	   sets which indicate which statements contain inserts, deletes,
	   or updates, and then examining intersections of these sets.
	*/

	temp_set = set_create(sset_sz);

	for (i = 1; i < n_exprs; i++) {
		if (set_ismember(candidates, i)) {
			p_node	ei = dflow_sets.exprs[i];
			int	j;

			for (j = 1; j < n_exprs; j++) {
				if (j == i) continue;
				if (set_ismember(candidates, j) &&
				    sub_expr(ei, dflow_sets.exprs[j], 0)) {
					set_copy(temp_set, deletes[i], sset_sz);
					if (set_intersect(temp_set,
							  deletes[i],
							  deletes[j],
							  sset_sz)) {
						continue;
					}
					set_copy(temp_set, inserts[i], sset_sz);
					if (set_intersect(temp_set,
							  inserts[i],
							  inserts[j],
							  sset_sz)) {
						continue;
					}
					set_copy(temp_set, ins_updates[i], sset_sz);
					if (set_intersect(temp_set,
							  ins_updates[i],
							  ins_updates[j],
							  sset_sz)) {
						continue;
					}
					break;
				}
			}
			if (j < n_exprs) {
				set_clrmember(candidates, i);
			}
		}
	}

	set_free(temp_set, sset_sz);

	for (i = 1; i < n_exprs; i++) {
		int	j;

		update_exprs[i] = 0;
		if (set_ismember(candidates, i)) {
			may_update[i] =
				  ! set_ismember(updates, i) ||
				  get_update_expr(dflow_sets.exprs[i], i, &update_exprs[i]);
			/* Check if we already have a similar expression with
			   the same update - and insertion points, so that we
			   can use that temporary.
			*/
			for (j = 1; j < i; j++) {
				t_node	*diff = 0;
				if (set_ismember(candidates, j) &&
				    similar_expr(j, i, &diff) &&
				    ! set_cmp(ins_updates[j], ins_updates[i], sset_sz) &&
				    ! set_cmp(inserts[i], inserts[j], sset_sz)) {
					handle_similar_expr(j, i, diff);
					break;
				}
				if (diff) kill_node(diff);
			}
			if (j == i) {
			    handle_possible_optim(i, may_update, update_exprs);
			}
		}
	}

	insert_new_stats(&statlist);

	for (i = 1; i < n_exprs; i++) {
		set_free(inserts[i], sset_sz);
		set_free(deletes[i], sset_sz);
		set_free(ins_updates[i], sset_sz);
		if (update_exprs[i]) kill_node(update_exprs[i]);
	}

	free(inserts);
	free(deletes);
	free(ins_updates);
	free(dflow_sets.ninserts);
	free(dflow_sets.nupdates);
	free(dflow_sets.ndeletes);
	free(new_stats);
	free(update_exprs);
	free(may_update);
	return statlist;
}

static int
similar_expr(n1, n2, pdiff)
	int	n1;
	int	n2;
	t_node	**pdiff;
{
/*
	t_node	*e1 = dflow_sets.exprs[n1];
	t_node	*e2 = dflow_sets.exprs[n2];
*/

	return 0;
}

static void
handle_similar_expr(orig, sim, diff)
	int	orig;
	int	sim;
	t_node	*diff;
{
	int	i;
	struct SR_sets
		*p;
	p_node	tmp;

	tmp = mk_leaf(Tmp, IDENT);
	tmp->nd_tmpvar = temporaries[orig];
	tmp->nd_type = temporaries[orig]->tmp_type;
	tmp = mk_expr(Oper, '+', tmp, node_copy(diff));
	tmp->nd_type = tmp->nd_left->nd_type;

	for (p = dflow_sets.sets, i = beginstatno;
	     i <= endstatno;
	     i++, p++) {
		if (set_ismember(p->delete_sndref, sim) &&
		    !replace_node(dflow_sets.stats[i],
				  dflow_sets.exprs[sim],
				  tmp,
				  (p_node *) 0,
				  0)) {
			assert(0);
		}
	}
}

static void
insert_list(plist, ind, adapt)
	p_node	*plist;
	int	ind;
	int	adapt;
{
	p_node	nd;
	p_node	l;

	node_walklist(new_stats[ind], l, nd) {
		node_fromlist(&new_stats[ind], nd);
		if (! *plist) {
			/* If the list is empty, put something in it. If we
			   don't, the order ends up wrong.
			*/
			p_node	n = mk_leaf(Stat, 0);

			node_insert(plist, n);
		}
		node_insert(plist, nd);
	}
	node_killlist(&new_stats[ind]);
	if (adapt) {
		l = *plist;
		while (node_nextlistel(node_prevlistel(l))) {
			l = node_prevlistel(l);
		}
		*plist = l;
	}
}

static void
insert_new_stats(plist)
	p_node	*plist;
{
	p_node	nd;
	p_node	l = *plist;

	if (new_stats[0]) {
		insert_list(&l, 0, 0);
	}

	node_walklist(l, l, nd) {
		p_node	n = nd;

		if (new_stats[nd->nd_nodenum]) {
			insert_list(&n, nd->nd_nodenum, 0);
		}
		if (nd->nd_class == Stat) {
			switch(nd->nd_symb) {
			case FOR:
				if (new_stats[nd->nd_o_info->o_ndnum[0]]) {
					insert_list(&n,
						nd->nd_o_info->o_ndnum[0], 0);
				}
				if (new_stats[nd->nd_o_info->o_ndnum[1]]) {
					insert_list(&(nd->nd_list2),
						nd->nd_o_info->o_ndnum[1], 1);
				}
				break;

			case GUARD:
				if (new_stats[nd->nd_o_info->o_ndnum[0]]) {
					insert_list(&(nd->nd_list1),
						nd->nd_o_info->o_ndnum[0], 1);
				}
				break;
			}

			if (nd->nd_symb != ARROW) {
				insert_new_stats(&(nd->nd_list1));
			}
			insert_new_stats(&(nd->nd_list2));
		}
	}
	l = *plist;
	while (node_nextlistel(node_prevlistel(l))) {
		l = node_prevlistel(l);
	}
	*plist = l;
}

static void
handle_possible_optim(expno, may_update, upd)
	int	expno;
	int	*may_update;
	p_node	*upd;
{
	int	i;
	p_node	e = dflow_sets.exprs[expno];
	struct SR_sets
		*p;
	p_node	initstat;

	/* First check position. If too difficult, don't bother.
	   Difficult positions are: behind expression in IF, CASE,
	   COND_EXIT, RETURN.
	*/
	for (p = dflow_sets.sets, i = beginstatno;
	     i <= endstatno;
	     i++, p++) {
		if (set_ismember(p->insert_sndref, expno) ||
		    set_ismember(p->insupd_fstref, expno)) {
			p_node	nd = dflow_sets.stats[i];

			if (! nd || nd->nd_nodenum == i) continue;
			switch(nd->nd_symb) {
			case COND_EXIT:
			case IF:
			case CASE:
			case RETURN:
				assert(i == nd->nd_o_info->o_ndnum[0]);
				set_clrmember(candidates, expno);
				return;
			}
		}
	}

	/* Now see how many deletes there are if we cannot update the
	   expression. If there are more updates than deletes it is better
	   to leave it as is.
	   Unfortunately, most checks must be moved anyway, because the
	   corresponding expression may be moved, and the check must be
	   done before the expression can be evaluated.
	*/
	if (! may_update[expno] &&
	    (e->nd_class != Check || e->nd_symb == ALIAS_CHK)) {
		if (dflow_sets.nupdates[expno] >= dflow_sets.ndeletes[expno]) {
			set_clrmember(candidates, expno);
			return;
		}
	}

	/* Now create a temporary for the expression, and an initialization
	   for it.
	*/
	switch(e->nd_class) {
	case Check:
		initstat = mk_leaf(Stat, CHECK);
		break;
	case Arrsel:
	case Ofldsel:
	case Oper:
	case Uoper:
	case Call:
		i = e->nd_class == Arrsel || e->nd_class == Ofldsel;
		initstat = mk_leaf(Stat,
				i ? TMPBECOMES : BECOMES);
		initstat->nd_desig = get_tmpvar(e->nd_type, i);
		temporaries[expno] = initstat->nd_desig->nd_tmpvar;
		break;
	default:
		set_clrmember(candidates, expno);
		return;
	}

	/* Now handle the deletes. This will also initialize initstat->nd_expr. */
	for (p = dflow_sets.sets, i = beginstatno;
	     i <= endstatno;
	     i++, p++) {
		if (set_ismember(p->delete_sndref, expno)) {
			do_delete(dflow_sets.stats[i], e, initstat);
		}
	}

	assert(initstat->nd_expr);
	initstat->nd_pos = initstat->nd_expr->nd_pos;
	if (initstat->nd_symb != CHECK) {
		initstat->nd_desig->nd_tmpvar->tmp_expr =
			node_copy(initstat->nd_expr);
	}

	/* Now handle the inserts and updates. */
	for (p = dflow_sets.sets, i = beginstatno;
	     i <= endstatno;
	     i++, p++) {
		if (set_ismember(p->insert_sndref, expno)) {
			node_enlist(&new_stats[i], node_copy(initstat));
		}
		if (set_ismember(p->insupd_fstref, expno)) {
			assert(dflow_sets.stats[i]->nd_symb == FOR_UPDATE);
			assert(dflow_sets.stats[i+1]->nd_symb == 0);
			if (may_update[expno] &&
			    initstat->nd_symb == CHECK &&
			    initstat->nd_expr->nd_symb == A_CHECK) {
				/* Special case for an update of an array bound
				   check: see if we can put an upper-bound
				   check in front of the loop instead.
				   We can if the loop has no exits or returns.
				*/
				int	j = i-1;
				p_node	u = dflow_sets.stats[i];
				p_node	n = 0;

				/* First find the loop header. */
				while (j) {
					n = dflow_sets.stats[j];
					if (n->nd_symb == FOR &&
					    n->nd_desig == u->nd_desig) {
						break;
					}
					j--;
				}
				assert(j);

				/* Now check that the loop does not exit or
				   return, and if so, replace.
				*/
				if (! (n->nd_flags & ND_EXIT_OR_RET)) {
				    p_node	c = node_copy(initstat);
				    if (replace_node(c, u->nd_desig, u->nd_expr->nd_right, (p_node *) 0, 1)) {
					node_enlist(&new_stats[n->nd_o_info->o_ndnum[1]], c);
					continue;
				    }
				    kill_node(c);
				}
			}
			do_update(i+1, e, initstat, initstat->nd_desig, may_update[expno], upd[expno]);
		}
	}

	kill_node(initstat);
}

static int
replace_node(nd, e, r, prepl, follow_temps)
	p_node	nd;
	p_node	e;
	p_node	r;
	p_node	*prepl;
	int	follow_temps;
{
	/*	In the tree indicated by 'nd', replace the expression
		or check indicated by 'e' with the temporary indicated by
		'r'. In case of a check, the check is deleted.
		Side effect: set *prepl if prepl is non-zero.
		Return 0 if the replacement could not be performed (because
		the expression indicated by 'e' was not found).
		If 'follow_temps' is set, we also replace in the expressions
		indicated by temporaries.
	*/

	p_node	n;
	p_node	l;
	int	ndnum = e->nd_nodenum;
	int	retval = 0;

	if (! nd) return 0;

	switch (nd->nd_class) {
	case Def:
		if (e->nd_class == Def && nd->nd_def == e->nd_def) {
			if (prepl && ! *prepl) {
				*prepl = node_copy(nd);
			}
			*nd = *r;
			return 1;
		}
		break;
	case Tmp:
		if (e->nd_class == Tmp && e->nd_tmpvar == nd->nd_tmpvar) {
			if (prepl && ! *prepl) {
				*prepl = node_copy(nd);
			}
			*nd = *r;
			return 1;
		}
		if (follow_temps && nd->nd_tmpvar->tmp_expr) {
			p_node	c = node_copy(nd->nd_tmpvar->tmp_expr);

			if (replace_node(c, e, r, prepl, follow_temps)) {
				*nd = *c;
				free_node(c);
				return 1;
			}
			kill_node(c);
		}
		break;

	case Oper:
	case Link:
	case Select:
	case Arrsel:
	case Ofldsel:
	case Uoper:
	case Check:
		if (e->nd_class == nd->nd_class && ndnum == nd->nd_nodenum) {
			if (prepl && ! *prepl) {
				*prepl = node_copy(nd);
			}
			kill_node(nd->nd_left);
			kill_node(nd->nd_right);
			r->nd_pos = nd->nd_pos;
			if (nd->nd_class == Check) {
				nd->nd_symb = 0;
				nd->nd_left = nd->nd_right = 0;
			}
			else {
				nd->nd_class = r->nd_class;
				nd->nd_type = r->nd_type;
				nd->nd_tmpvar = r->nd_tmpvar;
			}
			return 1;
		}
		retval = replace_node(nd->nd_left, e, r, prepl, follow_temps) |
			 replace_node(nd->nd_right, e, r, prepl, follow_temps);
		break;

	case Stat:
		retval = replace_node(nd->nd_expr, e, r, prepl, follow_temps) |
			 replace_node(nd->nd_desig, e, r, prepl, follow_temps);
		break;

	case Call:
		if (e->nd_class == nd->nd_class && ndnum == nd->nd_nodenum
		    && nd->nd_type) {
			if (prepl && ! *prepl) {
				*prepl = node_copy(nd);
			}
			kill_node(nd->nd_callee);
			kill_nodelist(nd->nd_parlist);
			nd->nd_callee = 0;
			nd->nd_parlist = 0;
			nd->nd_class = r->nd_class;
			nd->nd_type = r->nd_type;
			nd->nd_tmpvar = r->nd_tmpvar;
			r->nd_pos = nd->nd_pos;
			return 1;
		}
		retval = replace_node(nd->nd_callee, e, r, prepl, follow_temps);
		if (nd->nd_symb == '$') {
			retval |=
			 replace_node(nd->nd_obj, e, r, prepl, follow_temps);
		}
		node_walklist(nd->nd_parlist, l, n) {
			if (replace_node(n, e, r, prepl, follow_temps)) {
				retval = 1;
			}
		}
		break;

	case Aggr:
	case Row:
		node_walklist(nd->nd_memlist, l, n) {
			if (replace_node(n, e, r, prepl, follow_temps)) {
				retval = 1;
			}
		}
		break;
	}
	return retval;
}

static void
do_delete(nd, e, tmp)
	p_node	nd;
	p_node	e,
		tmp;
{
	/*	Replace the expression indicated by 'e' with 'tmp', in
		the statement indicated by 'nd'. This routine is also used
		to remove run-time checks.
	*/

	assert(tmp->nd_class == Stat);

	if (! replace_node(nd, e, tmp->nd_symb == CHECK ? tmp : tmp->nd_desig, &(tmp->nd_expr), 0)) {
		assert(0);
	}
}

static void
do_update(statno, e, init, tmp, may_update, upd)
	int	statno;
	p_node	e,
		init,
		upd,
		tmp;
	int	may_update;
{
	/*	Insert an update for the temporary indicated by "tmp" at
		the position indicated by "statno". The update is indicated
		by "upd". If "may_update" is not set, we have to insert
		a re-computation of the temporary instead. We must also do
		this if we don't know how to update. The computation is
		indicated by "init".
	*/

	if (may_update && (e->nd_class & (Uoper|Oper|Arrsel))) {
		p_node	nd = mk_leaf(Stat, UPDATE);

		nd->nd_pos = tmp->nd_pos;
		nd->nd_desig = node_copy(tmp);
		nd->nd_expr = node_copy(upd);
		node_enlist(&new_stats[statno], nd);
	}
	else {
		node_enlist(&new_stats[statno], node_copy(init));
	}
}

/* Here are the various meet and stat functions for all the data flow
   equations. no further comment, as these are derived directly from the
   equations.
*/

static int dsafe_meet(dst, s)
	int	dst;
	int	*s;
{
	set_copy(temp_set, full_set, eset_sz);
	while (*s != -1) {
		(void) set_intersect(
			temp_set,
			temp_set,
			dflow_sets.sets[*s++].dsafe_CM,
			eset_sz);
	}
	return set_assign(dflow_sets.sets[dst].dsafe_CM, temp_set, eset_sz);
}

static int dsafe_stat(src, dst)
	int	src, dst;
{
	(void) set_intersect(temp_set,
			     dflow_sets.sets[dst].trans,
			     dflow_sets.sets[src].dsafe_CM,
			     eset_sz);
	return set_union(dflow_sets.sets[dst].dsafe_CM,
			 temp_set,
			 dflow_sets.sets[dst].used,
			 eset_sz);
}

static int dsafe_SR_meet(dst, s)
	int	dst;
	int	*s;
{
	set_copy(temp_set, full_set, eset_sz);
	while (*s != -1) {
		(void) set_intersect(
			temp_set,
			temp_set,
			dflow_sets.sets[*s++].dsafe_SR,
			eset_sz);
	}
	return set_assign(dflow_sets.sets[dst].dsafe_SR, temp_set, eset_sz);
}

static int dsafe_SR_stat(src, dst)
	int	src, dst;
{
	(void) set_intersect(temp_set,
			     dflow_sets.sets[dst].SR_trans,
			     dflow_sets.sets[src].dsafe_SR,
			     eset_sz);
	return set_union(dflow_sets.sets[dst].dsafe_SR,
			 temp_set,
			 dflow_sets.sets[dst].used,
			 eset_sz);
}

static int earliest_meet(dst, s)
	int	dst;
	int	*s;
{
	set_init(temp_set, 0, eset_sz);
	set_init(temp_set2, 0, eset_sz);
	while (*s != -1) {
		(void) set_Xintersect(
			temp_set2,
			dflow_sets.sets[*s].dsafe_CM,
			dflow_sets.sets[*s].earliest_CM,
			eset_sz);
		(void) set_Xunion(
			temp_set2,
			dflow_sets.sets[*s++].trans,
			temp_set2,
			eset_sz);
		(void) set_union(
			temp_set,
			temp_set,
			temp_set2,
			eset_sz);
	}
	return set_assign(dflow_sets.sets[dst].earliest_CM, temp_set, eset_sz);
}

static int earliest_stat(src, dst)
	int	src, dst;
{
	(void) set_Xintersect(temp_set,
			 dflow_sets.sets[src].dsafe_CM,
			 dflow_sets.sets[src].earliest_CM,
			 eset_sz);
	return set_Xunion(dflow_sets.sets[dst].earliest_CM,
			 dflow_sets.sets[src].trans,
			 temp_set,
			 eset_sz);
}

static int earliest_SR_meet(dst, s)
	int	dst;
	int	*s;
{
	set_init(temp_set, 0, eset_sz);
	while (*s != -1) {
		(void) set_Xintersect(
			temp_set2,
			dflow_sets.sets[*s].dsafe_SR,
			dflow_sets.sets[*s].earliest_SR,
			eset_sz);
		(void) set_Xunion(
			temp_set2,
			dflow_sets.sets[*s++].SR_trans,
			temp_set2,
			eset_sz);
		(void) set_union(
			temp_set,
			temp_set,
			temp_set2,
			eset_sz);
	}
	return set_assign(dflow_sets.sets[dst].earliest_SR, temp_set, eset_sz);
}

static int earliest_SR_stat(src, dst)
	int	src, dst;
{
	(void) set_Xintersect(temp_set,
			 dflow_sets.sets[src].dsafe_SR,
			 dflow_sets.sets[src].earliest_SR,
			 eset_sz);
	return set_Xunion(dflow_sets.sets[dst].earliest_SR,
			 dflow_sets.sets[src].SR_trans,
			 temp_set,
			 eset_sz);
}

static int critical_meet(dst, s)
	int	dst;
	int	*s;
{
	set_init(temp_set, 0, eset_sz);
	while (*s != -1) {
		(void) set_union(
			temp_set,
			temp_set,
			dflow_sets.sets[*s++].critical,
			eset_sz);
	}
	return set_assign(dflow_sets.sets[dst].critical, temp_set, eset_sz);
}

static int critical_stat(src, dst)
	int	src, dst;
{
	(void) set_Xunion(temp_set,
			 dflow_sets.sets[dst].trans,
			 dflow_sets.sets[src].critical,
			 eset_sz);
	return set_Xintersect(dflow_sets.sets[dst].critical,
			      dflow_sets.sets[dst].used,
			      temp_set,
			      eset_sz);
}

static int subst_crit_meet(dst, s)
	int	dst;
	int	*s;
{
	set_init(temp_set, 0, eset_sz);
	while (*s != -1) {
		(void) set_Xintersect(
			temp_set2,
			dflow_sets.sets[*s].used,
			dflow_sets.sets[*s].subst_crit,
			eset_sz);
		(void) set_union(
			temp_set,
			temp_set,
			temp_set2,
			eset_sz);
		s++;
	}
	return set_union(dflow_sets.sets[dst].subst_crit,
			 temp_set,
			 dflow_sets.sets[dst].critins_SR,
			 eset_sz);
}

static int subst_crit_stat(src, dst)
	int	src, dst;
{
	(void) set_Xintersect(temp_set,
			      dflow_sets.sets[src].used,
			      dflow_sets.sets[src].subst_crit,
			      eset_sz);
	return set_union(dflow_sets.sets[dst].subst_crit,
			 temp_set,
			 dflow_sets.sets[dst].critins_SR,
			 eset_sz);
}

static int delay_meet(dst, s)
	int	dst;
	int	*s;
{
	set_copy(temp_set, full_set, eset_sz);
	while (*s != -1) {
		(void) set_Xintersect(
			temp_set2,
			dflow_sets.sets[*s].used,
			dflow_sets.sets[*s].delay,
			eset_sz);
		(void) set_intersect(
			temp_set,
			temp_set,
			temp_set2,
			eset_sz);
		s++;
	}
	return set_union(dflow_sets.sets[dst].delay,
			 dflow_sets.sets[dst].insert_fstref,
			 temp_set,
			 eset_sz);
}

static int delay_stat(src, dst)
	int	src, dst;
{
	(void) set_Xintersect(temp_set,
			      dflow_sets.sets[src].used,
			      dflow_sets.sets[src].delay,
			      eset_sz);
	return set_union(dflow_sets.sets[dst].delay,
			 temp_set,
			 dflow_sets.sets[dst].insert_fstref,
			 eset_sz);
}

static int isolated_meet(dst, s)
	int	dst;
	int	*s;
{
	set_copy(temp_set, full_set, eset_sz);
	while (*s != -1) {
		(void) set_Xintersect(
			temp_set2,
			dflow_sets.sets[*s].used,
			dflow_sets.sets[*s].isolated,
			eset_sz);
		(void) set_union(
			temp_set2,
			dflow_sets.sets[*s++].latest,
			temp_set2,
			eset_sz);
		(void) set_intersect(
			temp_set,
			temp_set,
			temp_set2,
			eset_sz);
	}
	return set_assign(dflow_sets.sets[dst].isolated, temp_set, eset_sz);
}

static int isolated_stat(src, dst)
	int	src, dst;
{
	(void) set_Xintersect(temp_set,
			      dflow_sets.sets[src].used,
			      dflow_sets.sets[src].isolated,
			      eset_sz);
	return set_union(dflow_sets.sets[dst].isolated,
			 temp_set,
			 dflow_sets.sets[src].latest,
			 eset_sz);
}

static int insupd_meet(dst, s)
	int	dst;
	int	*s;
{
	set_init(temp_set, 0, eset_sz);
	while (*s != -1) {
		(void) set_Xintersect(
			temp_set2,
			dflow_sets.sets[*s].insert_fstref,
			dflow_sets.sets[*s].insupd_fstref,
			eset_sz);
		(void) set_union(
			temp_set,
			temp_set,
			temp_set2,
			eset_sz);
		s++;
	}
	return set_assign(dflow_sets.sets[dst].insupd_fstref, temp_set, eset_sz);
}

static int insupd_stat(src, dst)
	int	src, dst;
{
	(void) set_Xintersect(temp_set,
			      dflow_sets.sets[src].insert_fstref,
			      dflow_sets.sets[src].insupd_fstref,
			      eset_sz);
	return set_union(dflow_sets.sets[dst].insupd_fstref,
			 dflow_sets.sets[dst].used,
			 temp_set,
			 eset_sz);
}
